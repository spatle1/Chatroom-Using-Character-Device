CC = gcc
PROJECT_DIR = .

OBJECTS = process.c
TARGET = process

$(TARGET) : $(OBJECTS)
	$(CC) -pthread -o $(TARGET) $(OBJECTS)
	
clean : 
	rm -rf $(TARGET)