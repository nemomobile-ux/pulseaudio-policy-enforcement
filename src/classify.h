#ifndef fooclassifyfoo
#define fooclassifyfoo

#include <sys/types.h>

#include "match.h"
#include "userdata.h"

#define PA_POLICY_PID_HASH_BITS  6
#define PA_POLICY_PID_HASH_MAX   (1 << PA_POLICY_PID_HASH_BITS)
#define PA_POLICY_PID_HASH_MASK  (PA_POLICY_PID_HASH_MAX - 1)

/* card flags */
#define PA_POLICY_DISABLE_NOTIFY            (1UL << 0)
#define PA_POLICY_NOTIFY_PROFILE_CHANGED    (1UL << 1)

/* stream flags */
#define PA_POLICY_LOCAL_ROUTE    (1UL << 0)
#define PA_POLICY_LOCAL_MUTE     (1UL << 1)
#define PA_POLICY_LOCAL_VOLMAX   (1UL << 2)

/* device flags */
#define PA_POLICY_REFRESH_PORT_ALWAYS (1UL << 3)
#define PA_POLICY_DELAYED_PORT_CHANGE (1UL << 4)

/* module flags */
#define PA_POLICY_MODULE_UNLOAD_IMMEDIATELY (1UL << 5)

/* module type */
#define PA_POLICY_MODULE_FOR_SINK   (0)
#define PA_POLICY_MODULE_FOR_SOURCE (1)
#define PA_POLICY_MODULE_COUNT      (2)

#define PA_POLICY_CARD_MAX_DEFS     (2)

struct pa_sink;
struct pa_source;
struct pa_sink_input;
struct pa_sink_input_new_data;
struct pa_card;

typedef struct pa_classify_app_id {
    pa_policy_match_object      *match;
    char                        *group;
} pa_classify_app_id;

struct pa_classify_stream_def {
    struct pa_classify_stream_def *next;
                                          /* for stream classification */
    pa_policy_match_object        *stream_match;
    uid_t                          uid;   /* user id, if any */
    char                          *exe;   /* exe name, if any */
    char                          *clnam; /* client name, if any */
    char                          *sname; /* active routing sink name, if any */
    uid_t                          sact;  /* routing sink active */
    char                          *group; /* policy group name */
    uint32_t                       flags; /* PA_POLICY_LOCAL_ROUTE |
                                             PA_POLICY_LOCAL_MUTE   */
    pa_proplist                   *properties;
};

struct pa_classify_stream {
    pa_hashmap                    *app_id_map;
    struct pa_classify_stream_def *defs;
};

struct pa_classify_port_config_entry {
    enum pa_classify_method      method;
    char                        *prop;
    char                        *arg;
    char                        *port_name;
};

struct pa_classify_port_entry {
    pa_policy_match_object      *device_match;
    char                        *port_name;
};

struct pa_classify_device_data {
    pa_idxset  *ports; /* Contains pa_classify_port_entry structs. If
                        * the device type doesn't require setting any ports,
                        * this is NULL. */
    char       *module;     /* If module is defined for device when device
                             * is activated that module is loaded. */
    char       *module_args;
    uint32_t    flags; /* PA_POLICY_DISABLE_NOTIFY, etc */
    uint32_t    port_change_delay;  /* Used if delayed port change is set */
};

struct pa_classify_device_def {
    char                            *type;  /* device type, e.g. ihf */
                                            /* for classification */
    pa_policy_match_object          *dev_match;
    struct pa_classify_device_data   data;  /* data associated with device */
};

struct pa_classify_device {
    int                              ndef;
    struct pa_classify_device_def    defs[1];
};

struct pa_classify_card_data {
    char                        *profile; /* name of profile */
    uint32_t                     flags;   /* PA_POLICY_DISABLE_NOTIFY, etc */
    pa_policy_match_object      *card_match;
};

struct pa_classify_card_def {
    char                        *type;    /* handled device name, e.g ihf */
    struct pa_classify_card_data data[2]; /* data associated with device 'type' */
};

struct pa_classify_card {
    int                          ndef;
    struct pa_classify_card_def  defs[1];
};

struct pa_classify_module {
    const char                  *module_name;
    const char                  *module_args;
    pa_module                   *module;
    uint32_t                     flags;
};

struct pa_classify {
    struct pa_classify_stream    streams;
    struct pa_classify_device   *sinks;
    struct pa_classify_device   *sources;
    struct pa_classify_card     *cards;
    struct pa_classify_module    module[PA_POLICY_MODULE_COUNT];
    pa_hook_slot                *module_unlink_hook_slot;
};

struct pa_classify_result {
    uint32_t    count;
    const char *types[1];
};

struct pa_classify *pa_classify_new(struct userdata *);
void  pa_classify_free(struct userdata *u);
void  pa_classify_add_sink(struct userdata *, const char *, const char *,
                           enum pa_classify_method, const char *, pa_idxset *,
                           const char *module, const char *module_args,
                           uint32_t flags, uint32_t port_change_delay);
void  pa_classify_add_source(struct userdata *, const char *, const char *,
                             enum pa_classify_method, const char *, pa_idxset *,
                             const char *module, const char *module_args,
                             uint32_t);
void  pa_classify_add_card(struct userdata *, char *,
                           enum pa_classify_method[PA_POLICY_CARD_MAX_DEFS], char **, char **,
                           uint32_t[PA_POLICY_CARD_MAX_DEFS]);
void  pa_classify_add_stream(struct userdata *, const char *,enum pa_classify_method,
                             const char *, const char *, const char *, uid_t, const char *, const char *,
                             uint32_t, const char *, const char *);
void  pa_classify_update_stream_route(struct userdata *u, const char *sname);

void  pa_classify_register_pid(struct userdata *, pid_t, const char *,
                               enum pa_classify_method, const char *, const char *);
void  pa_classify_unregister_pid(struct userdata *, pid_t, const char *,
                                 enum pa_classify_method, const char *);

void  pa_classify_register_app_id(struct userdata *, const char *, const char *,
                               enum pa_classify_method, const char *, const char *);
void  pa_classify_unregister_app_id(struct userdata *, const char *, const char *,
                                 enum pa_classify_method, const char *);

const char *pa_classify_sink_input(struct userdata *u, struct pa_sink_input *sinp,
                                   uint32_t *flags);
const char *pa_classify_sink_input_by_data(struct userdata *u,
                                           struct pa_sink_input_new_data *sinp,
                                           uint32_t *flags);
const char *pa_classify_source_output(struct userdata *u, struct pa_source_output *sout);
const char *pa_classify_source_output_by_data(struct userdata *u,
                                        struct pa_source_output_new_data *data);

int   pa_classify_sink(struct userdata *, struct pa_sink *,
                       uint32_t, uint32_t, struct pa_classify_result **result);
int   pa_classify_source(struct userdata *, struct pa_source *,
                         uint32_t, uint32_t, struct pa_classify_result **result);
int   pa_classify_card(struct userdata *, struct pa_card *,
                       uint32_t, uint32_t, bool, struct pa_classify_result **result);

int   pa_classify_card_all_types(struct userdata *u,
                                 struct pa_classify_result **result);
int   pa_classify_sink_all_types(struct userdata *u,
                                 struct pa_classify_result **result);
int   pa_classify_source_all_types(struct userdata *u,
                                   struct pa_classify_result **result);

int   pa_classify_is_sink_typeof(struct userdata *, struct pa_sink *,
                                 const char *,
                                 struct pa_classify_device_data **);
int   pa_classify_is_source_typeof(struct userdata *, struct pa_source *,
                                   const char *,
                                   struct pa_classify_device_data **);
int   pa_classify_is_card_typeof(struct userdata *, struct pa_card *,
                                 const char *, struct pa_classify_card_data **, int *priority);

/* The ports= option in the [device] section may contain multiple sinks or
 * sources of which port should be set. These two functions are used to find
 * out whether the port of the given sink or source should be set. */
int pa_classify_is_port_sink_typeof(struct userdata *, struct pa_sink *,
                                    const char *,
                                    struct pa_classify_device_data **);
int pa_classify_is_port_source_typeof(struct userdata *, struct pa_source *,
                                      const char *,
                                      struct pa_classify_device_data **);
struct pa_classify_port_entry *pa_classify_get_port_entry(struct pa_classify_device_data *,
                                                          enum pa_policy_object_type,
                                                          void *);

int pa_classify_update_module(struct userdata *u, uint32_t dir, struct pa_classify_device_data *device);
void pa_classify_update_modules(struct userdata *u, uint32_t dir, const char *type);

#endif


/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
