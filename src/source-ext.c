#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <pulse/def.h>

#include <pulsecore/core-util.h>
#include <pulsecore/source.h>

#include "source-ext.h"
#include "classify.h"
#include "context.h"
#include "policy-group.h"
#include "dbusif.h"
#include "policy.h"
#include "log.h"

/* hooks */
static pa_hook_result_t source_put(void *, void *, void *);
static pa_hook_result_t source_unlink(void *, void *, void *);

static void handle_new_source(struct userdata *, struct pa_source *);
static void handle_removed_source(struct userdata *, struct pa_source *);



struct pa_source_evsubscr *pa_source_ext_subscription(struct userdata *u)
{
    pa_core                   *core;
    pa_hook                   *hooks;
    struct pa_source_evsubscr *subscr;
    pa_hook_slot              *put;
    pa_hook_slot              *unlink;
    
    pa_assert(u);
    pa_assert_se((core = u->core));

    hooks  = core->hooks;
    
    put    = pa_hook_connect(hooks + PA_CORE_HOOK_SOURCE_PUT,
                             PA_HOOK_LATE, source_put, (void *)u);
    unlink = pa_hook_connect(hooks + PA_CORE_HOOK_SOURCE_UNLINK,
                             PA_HOOK_LATE, source_unlink, (void *)u);


    subscr = pa_xnew0(struct pa_source_evsubscr, 1);
    
    subscr->put    = put;
    subscr->unlink = unlink;
    
    return subscr;
}

void pa_source_ext_subscription_free(struct pa_source_evsubscr *subscr)
{
    if (subscr != NULL) {
        pa_hook_slot_free(subscr->put);
        pa_hook_slot_free(subscr->unlink);

        pa_xfree(subscr);
    }
}

void pa_source_ext_discover(struct userdata *u)
{
    void             *state = NULL;
    pa_idxset        *idxset;
    struct pa_source *source;

    pa_assert(u);
    pa_assert(u->core);
    pa_assert_se((idxset = u->core->sources));

    while ((source = pa_idxset_iterate(idxset, &state, NULL)) != NULL)
        handle_new_source(u, source);
}


const char *pa_source_ext_get_name(struct pa_source *source)
{
    return source->name ? source->name : "<unknown>";
}

int pa_source_ext_set_mute(struct userdata *u, const char *type, int mute)
{
    void              *state = NULL;
    pa_idxset         *idxset;
    struct pa_source  *source;
    const char        *name;
    bool          current_mute;

    pa_assert(u);
    pa_assert(type);
    pa_assert(u->core);
    pa_assert_se((idxset = u->core->sources));

    while ((source = pa_idxset_iterate(idxset, &state, NULL)) != NULL) {
        if (pa_classify_is_source_typeof(u, source, type, NULL)) {
            name = pa_source_ext_get_name(source);
            current_mute = pa_source_get_mute(source, 0);

            if ((current_mute && mute) || (!current_mute && !mute)) {
                pa_log_debug("%s() source '%s' type '%s' is already %smuted",
                             __FUNCTION__, name, type, mute ? "" : "un");
            }
            else {
                pa_log_debug("%s() %smute source '%s' type '%s'",
                             __FUNCTION__, mute ? "" : "un", name, type);

                pa_source_set_mute(source, mute, true);
            }

            return 0;
        }
    }


    return -1;
}

int pa_source_ext_set_ports(struct userdata *u, const char *type)
{
    int ret = 0;
    pa_source *source;
    struct pa_classify_device_data *data;
    uint32_t idx;

    pa_assert(u);
    pa_assert(u->core);

    pa_classify_update_modules(u, PA_POLICY_MODULE_FOR_SOURCE, type);

    PA_IDXSET_FOREACH(source, u->core->sources, idx) {
        /* Check whether the port of this source should be changed. */
        if (pa_classify_is_port_source_typeof(u, source, type, &data)) {
            struct pa_classify_port_entry *port_entry;

            pa_assert_se(port_entry = pa_classify_get_port_entry(data,
                                                                 pa_policy_object_source,
                                                                 source));

            pa_classify_update_module(u, PA_POLICY_MODULE_FOR_SOURCE, data);

            if (!source->active_port ||
                    !pa_streq(port_entry->port_name,
                              source->active_port->name)) {

                if (pa_source_set_port(source, port_entry->port_name,
                                       false) < 0) {
                    ret = -1;
                    pa_log("failed to set source '%s' port to '%s'",
                           source->name, port_entry->port_name);
                }
                else {
                    pa_log_debug("changed source '%s' port to '%s'",
                                 source->name, port_entry->port_name);
                }
                continue;
            }

            if (data->flags & PA_POLICY_REFRESH_PORT_ALWAYS) {
                if (source->set_port) {
                    pa_log_debug("refresh source '%s' port to '%s'",
                            source->name, port_entry->port_name);
                    source->set_port(source, source->active_port);
                }
                continue;
            }
        }
    }

    return ret;
}

static pa_hook_result_t source_put(void *hook_data, void *call_data,
                                       void *slot_data)
{
    struct pa_source  *source = (struct pa_source *)call_data;
    struct userdata *u    = (struct userdata *)slot_data;

    handle_new_source(u, source);

    return PA_HOOK_OK;
}


static pa_hook_result_t source_unlink(void *hook_data, void *call_data,
                                          void *slot_data)
{
    struct pa_source  *source = (struct pa_source *)call_data;
    struct userdata *u = (struct userdata *)slot_data;

    handle_removed_source(u, source);

    return PA_HOOK_OK;
}

static void handle_new_source(struct userdata *u, struct pa_source *source)
{
    const char      *name;
    uint32_t         idx;
    char            *buf;
    int              ret;
    struct pa_classify_result *r;

    if (source && u) {
        name = pa_source_ext_get_name(source);
        idx  = source->index;

        if (pa_streq(name, u->nullsource->name)) {
            u->nullsource->source = source;
            pa_log_debug("new source '%s' (idx=%d) will be used to "
                         "mute-by-route", name, idx);
        }

        if (pa_policy_log_level_debug()) {
            pa_classify_source(u, source, 0, 0, &r);
            buf = pa_policy_log_concat(r->types, r->count);
            ret = pa_proplist_sets(source->proplist,
                                   PA_PROP_POLICY_DEVTYPELIST, buf);

            if (ret < 0)
                pa_log("failed to set property '%s' on source '%s'",
                       PA_PROP_POLICY_DEVTYPELIST, name);

            pa_log_debug("new source '%s' (idx=%d%s%s)",
                         name, idx, r->count > 0 ? ", type=" : "", buf);
            pa_xfree(buf);
            pa_xfree(r);
        }

        pa_policy_context_register(u,pa_policy_object_source,name,source);
#if 0
        pa_policy_groupset_update_default_source(u, PA_IDXSET_INVALID);
#endif
        pa_policy_groupset_register_source(u, source);

        pa_classify_source(u, source, PA_POLICY_DISABLE_NOTIFY, 0, &r);
        pa_policy_send_device_state(u, PA_POLICY_CONNECTED, r);
        pa_xfree(r);

        pa_policy_groupset_update_sources(u);
    }
}

static void handle_removed_source(struct userdata *u, struct pa_source *source)
{
    const char      *name;
    uint32_t         idx;
    char            *buf;
    struct pa_null_source     *ns;
    struct pa_classify_result *r;

    if (source && u) {
        name = pa_source_ext_get_name(source);
        idx  = source->index;
        ns   = u->nullsource;

        if (ns->source == source) {
            pa_log_debug("cease to use source '%s' (idx=%u) to mute-by-route",
                         name, idx);
            ns->source = NULL;
        }

        pa_policy_context_unregister(u, pa_policy_object_source,
                                     name, source, idx);

        if (pa_policy_log_level_debug()) {
            pa_classify_source(u, source, 0, 0, &r);
            buf = pa_policy_log_concat(r->types, r->count);
            pa_log_debug("remove source '%s' (idx=%d%s%s)",
                         name, idx, r->count > 0 ? ", type=" : "", buf);
            pa_xfree(buf);
            pa_xfree(r);
        }

#if 0
        pa_policy_groupset_update_default_source(u, idx);
#endif
        pa_policy_groupset_unregister_source(u, idx);

        pa_classify_source(u, source, PA_POLICY_DISABLE_NOTIFY, 0, &r);
        pa_policy_send_device_state(u, PA_POLICY_DISCONNECTED, r);
        pa_xfree(r);
    }
}

struct pa_null_source *pa_source_ext_init_null_source(const char *name)
{
    struct pa_null_source *null_source = pa_xnew0(struct pa_null_source, 1);

    /* source.null is temporary to de-couple PA releases from ours */
    null_source->name = pa_xstrdup(name ? name : /* "null" */ "source.null");
    null_source->source = NULL;

    return null_source;
}

void pa_source_ext_null_source_free(struct pa_null_source *null_source)
{
    if (null_source) {
        pa_xfree(null_source->name);
        pa_xfree(null_source);
    }
}




/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
