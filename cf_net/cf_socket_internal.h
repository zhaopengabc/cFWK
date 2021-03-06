#ifndef CF_SOCKET_INTERNAL_H
#define CF_SOCKET_INTERNAL_H

typedef struct cf_socket cf_socket;
typedef struct cf_socket_inf {
    int (*run)(cf_socket* );
    int (*write)(cf_socket* ,const uint8_t* ,size_t );
}cf_socket_inf;
typedef struct cf_socket {
    const cf_socket_inf* inf;
    void* instance;
    void* user_data;
    void (*on_new_socket)(cf_socket* ,cf_socket* );
    void (*on_disconnect)(cf_socket* ,cf_socket* );
    void (*on_client_read)(cf_socket* ,const uint8_t*,size_t len );
}cf_socket;
#endif//CF_SOCKET_INTERNAL_H