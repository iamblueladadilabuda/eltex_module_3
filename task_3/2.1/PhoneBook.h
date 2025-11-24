// PhoneBook.h
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#define N 100
#define LINE_SIZE 50

// Структура информации о человеке
typedef struct Man
{
	int id;
	char first_name[LINE_SIZE];
	char second_name[LINE_SIZE];
	char third_name[LINE_SIZE];
	char place_of_work[LINE_SIZE];
	char post[LINE_SIZE];
} Man;

// Структура контактной информации
typedef struct Contact
{
	char phone_number[N][LINE_SIZE];
	char email[N][LINE_SIZE];
	char references[N][LINE_SIZE];
} Contact;

// Структура записи в телефонной книге
typedef struct Phone_Book
{
	Man man;
	Contact contact;
} Phone_Book;

Phone_Book phone_book[N]; // Телефонная книга
int arr_size; // Размер телефонной книги (заполненная часть)

int Insert(int fd); // Добавить
int Find(int num_id); // Поиск строки по ID
int Redact(int fd, int index); // Редактировать
int Delete(int fd, int index); // Удалить
void Print(); // Печать

// Нужно для добавления
Man Man_Init(); // Инициализация man
Contact Contact_Init(); // Инициализация contact
int Enter_ID(); // Ввод поля ID
void Enter_Man(char* line, const char* data); // Ввод остальных полей man
void Enter_Contact(char contact_arr[N][LINE_SIZE], const char* data); // Ввод полей contact

// Для редактирования
int Check_Redact(char* old_line, char* new_line); // Проверка корректности изменения полей

// Нужно для печати
void Print_Line(int i); // Печать одной строки массива
void Print_Contact(char contact_arr[N][LINE_SIZE], const char* data); // Печать contact
