/*
 *  MsgLog.h
 *
 *
 *  Created by Slice on 10.04.12.
 *  Copyright 2012 Home. All rights reserved.
 *
 */

#ifndef __MESSAGE_LOG_H__
#define __MESSAGE_LOG_H__

#define BOOTER_LOG_SIZE (4 * 1024)

#define BootLog(...) \
  if (msgCursor) { \
    AsciiSPrint (msgCursor, BOOTER_LOG_SIZE, __VA_ARGS__); \
    while (*msgCursor) { \
      msgCursor++; \
    } \
    Msg->Cursor = msgCursor; \
  }

typedef struct {
  UINT32    SizeOfLog;
  BOOLEAN   Dirty;
  CHAR8     *Log;
  CHAR8     *Cursor;
} MESSAGE_LOG_PROTOCOL;

#endif
