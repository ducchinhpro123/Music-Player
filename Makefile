CC = gcc
CFLAGS = -Wall -I/usr/local/include
LDFLAGS = -L/home/chinhcom/kissfft -lkissfft-float -lraylib -lm

# Define the library path
LIBRARY_PATH = /home/chinhcom/kissfft

TARGET = main
SOURCES = main.c

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) $(SOURCES) -o $(TARGET) $(LDFLAGS)

run: $(TARGET)
	LD_LIBRARY_PATH=$(LIBRARY_PATH) ./$(TARGET)


clean:
	rm -f $(TARGET)
