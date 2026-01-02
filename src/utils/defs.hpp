#pragma once

#ifdef SERVER
#define SERVER_ONLY(...) __VA_ARGS__
#else
#define SERVER_ONLY(...)
#endif // SERVER

#ifdef CLIENT
#define CLIENT_ONLY(...) __VA_ARGS__
#else
#define CLIENT_ONLY(...)
#endif

//#define MSGDEBUG(...) __VA_ARGS__
#define MSGDEBUG(...)

#define DELETE_COPY_MOVE(classname) \
    classname(const classname& other) = delete; \
    classname(classname&& other) = delete; \
    classname& operator=(const classname& other) = delete; \
    classname& operator=(classname&& other) = delete;

