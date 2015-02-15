
// TODO implement sam_msg (#35)

/// request types
typedef enum {
    SAM_MSG_REQ_PUBLISH,
    SAM_MSG_REQ_EXCH_DECLARE,
    SAM_MSG_REQ_EXCH_DELETE
} sam_msg_req_t;


/// response types
typedef enum {
    SAM_MSG_RES_ACK,
    SAM_MSG_RES_CONNECTION_LOSS
} sam_msg_res_t;



typedef struct sam_msg_t {
    sam_logger_t *logger;
    zactor_t *actor;
} sam_msg_t;


/// return type for "start" functions
/// of different message backends
typedef struct sam_msg_backend_t {
    void *self;
    zsock_t *req;
    zactor_t *actor;
} sam_msg_backend_t;


//  --------------------------------------------------------------------------
/// @brief Creates a new sam_msg instance
/// @return Handle for inter thread communication
sam_msg_t *
sam_msg_new ();


//  --------------------------------------------------------------------------
/// @brief Save and publish a message
void
sam_msg_destroy (
    sam_msg_t **self);


//  --------------------------------------------------------------------------
/// @brief Self test this class
void
sam_msg_test ();
