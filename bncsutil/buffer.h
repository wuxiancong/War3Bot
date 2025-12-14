#ifndef BUFFER_H
#define BUFFER_H

#include <bncsutil/mutil.h>

typedef struct msg_buffer *msg_buffer_t;
typedef struct msg_buffer_impl *msg_buffer_impl_t;
typedef struct msg_reader *msg_reader_t;
typedef struct msg_reader_impl *msg_reader_impl_t;

// Callbacks
typedef size_t __stdcall (*buffer_bytes_to_reserve)();
typedef int __stdcall (*buffer_create_header)(msg_buffer_t);


msg_buffer_t create_buffer(size_t initial_size);
msg_buffer_t create_bncs_buffer(size_t initial_size);
void destroy_buffer(msg_buffer_t);

// Adders
void buffer_add_8(msg_buffer_t, int8_t);
void buffer_add_u8(msg_buffer_t, uint8_t);
void buffer_add_16(msg_buffer_t, int16_t);
void buffer_add_u16(msg_buffer_t, uint16_t);
void buffer_add_32(msg_buffer_t, int32_t);
void buffer_add_u32(msg_buffer_t, uint32_t);

msg_reader_t create_reader(size_t initial_size);

#endif
