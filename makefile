# Компилятор и флаги
CC = gcc
CFLAGS = -Iinclude -Wall -g

# Папки
SRCDIR = server/src
INCDIR = server/include
OBJDIR = obj
BINDIR = bin

# Название выходного файла
TARGET = server

# Исходные файлы
SRCS = $(wildcard $(SRCDIR)/*.c)
# Объектные файлы
OBJS = $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.o, $(SRCS))

# Рецепт по умолчанию
all: $(BINDIR)/$(TARGET)

# Компиляция и создание исполняемого файла
$(BINDIR)/$(TARGET): $(OBJS)
	@mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $^

# Компиляция объектных файлов
$(OBJDIR)/%.o: $(SRCDIR)/%.c $(INCDIR)/handle_client.h
	@mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Очистка скомпилированных файлов
clean:
	rm -rf $(OBJDIR) $(BINDIR)

.PHONY: all clean