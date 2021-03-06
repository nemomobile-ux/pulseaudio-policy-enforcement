#ifndef foopolicycontextfoo
#define foopolicycontextfoo

#include "classify.h"
#include "match.h"

#define PA_POLICY_CONTEXT_MAX_CHANGES (16)

enum pa_policy_action_type {
    pa_policy_action_unknown = 0,
    pa_policy_action_min = pa_policy_action_unknown,

    pa_policy_set_property,
    pa_policy_delete_property,
    pa_policy_set_default,
    pa_policy_override,

    pa_policy_action_max
};

enum pa_policy_value_type {
    pa_policy_value_unknown = 0,
    pa_policy_value_min = pa_policy_value_unknown,

    pa_policy_value_constant, /* constant value */
    pa_policy_value_copy,     /* copy of the value of context var. */

    pa_policy_value_max
};

struct pa_policy_object {
    enum pa_policy_object_type          type;
    pa_policy_match_object             *match;
    void                               *ptr;
    unsigned long                       index;
};

struct pa_policy_value_constant {
    enum pa_policy_value_type           type;
    char                               *string;    
};

struct pa_policy_value_copy {
    enum pa_policy_value_type           type;
};

union pa_policy_value {
    enum pa_policy_value_type           type;
    struct pa_policy_value_constant     constant;
    struct pa_policy_value_copy         copy;
};

#define PA_POLICY_CONTEXT_ACTION_COMMON         \
    union pa_policy_context_action     *next;   \
    int                                 lineno; \
    enum pa_policy_action_type          type

struct pa_policy_context_action_any {
    PA_POLICY_CONTEXT_ACTION_COMMON;
};


struct pa_policy_set_property {
    PA_POLICY_CONTEXT_ACTION_COMMON;
    struct pa_policy_object             object;
    char                               *property;
    union pa_policy_value               value;
};

struct pa_policy_del_property {
    PA_POLICY_CONTEXT_ACTION_COMMON;
    struct pa_policy_object             object;
    char                               *property;
};

struct pa_policy_set_default {
    PA_POLICY_CONTEXT_ACTION_COMMON;
    struct pa_policy_activity_variable *var;
    int                                 default_state;
};

struct pa_policy_override {
    PA_POLICY_CONTEXT_ACTION_COMMON;
    struct pa_policy_object             object;
    char                               *orig_profile;
    char                               *profile;
    char                               *active_val;
    union pa_policy_value               value;
    int                                 active;
};

union pa_policy_context_action {
    struct pa_policy_context_action_any any;
    struct pa_policy_set_property       setprop;
    struct pa_policy_del_property       delprop;
    struct pa_policy_set_default        setdef;
    struct pa_policy_override           overr;
};

struct pa_policy_context_rule {
    struct pa_policy_context_rule      *next;
    pa_policy_match_object             *match;
    union pa_policy_context_action     *actions;
};

struct pa_policy_context_variable {
    struct pa_policy_context_variable  *next;
    char                               *name;
    char                               *value;
    struct pa_policy_context_rule      *rules;
};

struct pa_policy_activity_rule {
    struct pa_policy_activity_rule     *next;
    pa_policy_match_object             *match;
    union pa_policy_context_action     *actions;
};

struct pa_policy_activity_variable {
    struct pa_policy_activity_variable *next;
    char                               *device;
    struct pa_policy_context_rule      *active_rules;
    struct pa_policy_context_rule      *inactive_rules;
    struct userdata                    *userdata;
    /* activity variable is active if hook slot pointer is not-null */
    pa_hook_slot                       *sink_state_changed_hook_slot;
    int                                 default_state; /* -1 select based on sink running/suspended,
                                                          1 active, 0 inactive */
    /* cache some values when variable is active */
    int                                 sink_opened; /* -1 not set, 0 closed, 1 opened */
};

struct pa_policy_context {
    struct pa_policy_context_variable  *variables;
    struct pa_policy_activity_variable *activities;
    struct variable_change {
        union pa_policy_context_action *action;
        char                            *value;
    } variable_change[PA_POLICY_CONTEXT_MAX_CHANGES];
    int                                 variable_change_count;
    union pa_policy_context_action     *overrides;
};


struct pa_policy_context *pa_policy_context_new(struct userdata *);
void pa_policy_context_free(struct pa_policy_context *);

void pa_policy_context_register(struct userdata *, enum pa_policy_object_type,
                                const char *, void *);
void pa_policy_context_unregister(struct userdata *,enum pa_policy_object_type,
                                  const char *, void *, unsigned long);


struct pa_policy_context_rule
    *pa_policy_context_add_property_rule(struct userdata *, const char *,
                                         enum pa_classify_method, const char *);

void pa_policy_context_add_property_action(struct userdata *,
                                           struct pa_policy_context_rule *,int,
                                           enum pa_policy_object_type,
                                           enum pa_classify_method, const char *,
                                           const char *,
                                           enum pa_policy_value_type, ...);

void pa_policy_context_delete_property_action(struct userdata *,
                                              struct pa_policy_context_rule *,
                                              int,
                                              enum pa_policy_object_type,
                                              enum pa_classify_method,
                                              const char *, const char *);

void pa_policy_context_set_default_action(struct pa_policy_context_rule *rule,
                                          int lineno,
                                          struct userdata *u,
                                          const char *activity_group,
                                          int default_state);

void pa_policy_context_override_action(struct userdata *u,
                                       struct pa_policy_context_rule *,int,
                                       enum pa_policy_object_type,
                                       enum pa_classify_method, const char *,
                                       const char *,
                                       enum pa_policy_value_type, ...);

int pa_context_override_card_profile(struct userdata *u,
                                     pa_card *card,
                                     const char *pn,
                                     const char **override_pn);

/* collect context variable change as name and value. */
int pa_policy_context_variable_changed(struct userdata *u, const char *name, const char *value);
/* commit variable changes to proplists. this needs to be always called after adding
 * values with pa_policy_context_variable_changed() */
void pa_policy_context_variable_commit(struct userdata *u);

/* device       - device mode, bta2dp, bthsp, etc.
 * sink_name    - active sink on device mode, matched with method & arg
 */

void pa_policy_activity_add(struct userdata *u, const char *device);

struct pa_policy_context_rule
    *pa_policy_activity_add_active_rule(struct userdata *u, const char *device,
                                         enum pa_classify_method method, const char *sink_name);

struct pa_policy_context_rule
    *pa_policy_activity_add_inactive_rule(struct userdata *u, const char *device,
                                          enum pa_classify_method method, const char *sink_name);

int pa_policy_activity_device_changed(struct userdata *u, const char *device);

void pa_policy_activity_register(struct userdata *, enum pa_policy_object_type,
                                 const char *, void *);
void pa_policy_activity_unregister(struct userdata *,enum pa_policy_object_type,
                                   const char *, void *, unsigned long);

#endif

/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
