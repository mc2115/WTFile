#ifndef __MESSAGE_TYPE__
#define __MESSAGE_TYPE__
//types of messages passed between client and server
//In our protocol, a short integer is used to pass the message type
//Instead of a string
enum MessageType
{
    CONFIGURE = 130, //only used by client
    CHECKOUT,
    UPDATE,
    UPGRADE,
    COMMIT,
    PUSH,
    CREATE,
    DESTROY,
    ADD,    //only used by client
    REMOVE, //only used by client
    CURRENTVERSION,
    HISTORY,
    ROLLBACK,
    ONEFILE,
    FILES,
    SUCCESS,
    FAIL
};
const char *msgType(int mtype)
{
    const char *types[] = { "configure", "checkout", "update", "upgrade", "commit", "push", "create", "destroy", "add",
                            "remove", "currentversion", "history", "rollback", "onefile", "files", "success", "fail" };
    if (mtype >= CONFIGURE && mtype <= FAIL)
    {
        return types[mtype - CONFIGURE];
    }
    else
    {
        return "Unknown message type";
    }
}
#endif
