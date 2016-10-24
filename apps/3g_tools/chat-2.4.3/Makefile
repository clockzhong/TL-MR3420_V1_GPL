# $Id: Makefile,v 1.40 2006/02/13 03:06:25 quozl Exp $
VERSION=0.9.0
RELEASE=

CC = mips-linux-gcc
RM	= rm -f
CFLAGS  = -Wall -Wno-trigraphs -O2 -fomit-frame-pointer

#TP_WEB_LOG=y

ifdef TP_WEB_LOG
CFLAGS += -DTP_WEB_LOG
endif

ifdef TP_WEB_LOG
LDFLAGS += -lmsglog
endif

CHAT_BIN = chat

CHAT_OBJS = chat.o 

all: $(CHAT_BIN)

$(CHAT_BIN): $(CHAT_OBJS) 
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(CHAT_BIN) $(CHAT_OBJS) $(LIBS)

clean:
	$(RM) *.o $(CHAT_BIN) 

