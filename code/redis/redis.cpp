#include <iostream>
#include <hiredis/hiredis.h>

int main() {
    // 连接到本地的Redis服务器
    redisContext *context = redisConnect("127.0.0.1", 6379);
    if (context == nullptr || context->err) {
        if (context) {
            std::cerr << "Connection error: " << context->errstr << std::endl;
            redisFree(context);
        } else {
            std::cerr << "Connection error: Can't allocate redis context" << std::endl;
        }
        return 1;
    }

    // 设置和获取值
    const char *key = "mykey";
    const char *value = "Hello, Redis!";
    
    // 设置值
    redisReply *reply = static_cast<redisReply*>(redisCommand(context, "SET %s %s", key, value));
    if (reply == nullptr) {
        std::cerr << "SET error" << std::endl;
        redisFree(context);
        return 1;
    }
    freeReplyObject(reply);

    // 获取值
    reply = static_cast<redisReply*>(redisCommand(context, "GET %s", key));
    if (reply == nullptr) {
        std::cerr << "GET error" << std::endl;
        redisFree(context);
        return 1;
    }
    if (reply->type == REDIS_REPLY_STRING) {
        std::cout << "GET " << key << ": " << reply->str << std::endl;
    }
    freeReplyObject(reply);

    // 断开连接
    redisFree(context);

    return 0;
}
