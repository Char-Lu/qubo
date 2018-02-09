/* Jeremy Weed
 * R@M 2018
 * jweed262@umd.edu
 */

/*
 * Declarations for intertask message/stream buffers
 */

#ifndef _INTERTASK_MESSAGES_H_
#define _INTERTASK_MESSAGES_H_

#include <FreeRTOS.h>
#include <stream_buffers.h>
#include <queue.h>
#include <semphr.h>


extern MessageBufferHandle_t thruster_message_buffer;

#define INIT_MESSAGE_BUFFERS() do {							\
		thruster_message_buffer = xMessageBufferCreate(sizeof(Thruster_Set)); \
	} while (0)


#endif
