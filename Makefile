.ifndef RELEASE
 CFLAGS += -DDEBUG -g
.endif

PROG := ./src/mailvalidator 
OBJ := ./src/mailvalidator.o ./src/printlog.o ./src/sckt.o ./src/mxlookup.o ./src/queue.o ./src/smtpclient.o ./src/textsearch.o

$(PROG): $(OBJ)
	c++ -o $(.TARGET) $(.ALLSRC) $(CFLAGS) -lpthread

depend:
	c++ -E -MM *.cc > .depend

clean:
	rm -f $(PROG) *.o
