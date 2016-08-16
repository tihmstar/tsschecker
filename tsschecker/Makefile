TARGET = cydiatsschecker
CFLAGS += -std=c11
LDFLAGS += -I /usr/local/include -lcurl -lbsd -lm -lplist -L /usr/local/lib/ -lpartialzip-1.0

all : $(TARGET)

cydiatsschecker : main.o download.o jsmn.o tss.o tsschecker.o
		$(CC) $(CFLAGS) main.o download.o jsmn.o tss.o tsschecker.o $(LDFLAGS) -o $(TARGET)
		@echo "Successfully built $(TARGET)"

main.o : main.c
		$(CC) $(CFLAGS) -c main.c -o main.o

download.o : download.c 
		$(CC) $(CFLAGS) -c download.c -o download.o

jsmn.o : jsmn.c
		$(CC) $(CFLAGS) -c jsmn.c -o jsmn.o

tss.o : tss.c
		$(CC) $(CFLAGS) -c tss.c -o tss.o

tsschecker.o :
		$(CC) $(CFLAGS) -c tsschecker.c -o tsschecker.o

install :
		cp $(TARGET) /usr/local/bin/
		@echo "Installed $(TARGET)"
clean :
		rm -rf *.o $(TARGET)	
