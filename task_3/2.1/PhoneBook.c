#include "PhoneBook.h"

int Insert(int fd)
{
	if (arr_size == N) return -1; // Ошибка!

	Phone_Book book;

	// Инициализация полей man и contact
	book.man = Man_Init();
	book.contact = Contact_Init();

	// Добавляем запись в телефонную книгу
	phone_book[arr_size] = book;
	arr_size += 1;

	if (lseek(fd, 0, SEEK_SET) < 0) 
	{
		perror("Ошибка lseek при записи");
		return -1;
	}

	return write(fd, phone_book, sizeof(Phone_Book) * arr_size);
}

Man Man_Init()
{
	Man man;

	man.id = Enter_ID();
	Enter_Man(man.first_name, "фамилию");
	Enter_Man(man.second_name, "имя");
	Enter_Man(man.third_name, "отчество");
	Enter_Man(man.place_of_work, "место работы");
	Enter_Man(man.post, "должность");

	return man;
}

Contact Contact_Init()
{
	Contact contact;

	Enter_Contact(contact.phone_number, "номера телефонов");
	Enter_Contact(contact.email, "адреса почт");
	Enter_Contact(contact.references, "ссылки на страницы в соцсетях");

	return contact;
}

int Enter_ID()
{
	int id, count_id = 0;
	printf("Введите ID: ");
	scanf_s("%d", &id);

	if (arr_size == 0 && id > 0)
	{
		return id;
	}

	while (1)
	{
		while (id <= 0) // Проверка, что ID > 0
		{
			printf("ID должен быть больше 0!\n");
			printf("Введите ID: ");
			scanf_s("%d", &id);
		}

		for (int i = 0; i < arr_size; i++) // Проверка, что ID ещё нет в книге
		{
			if (phone_book[i].man.id == id) // Сравнение с каждым ID в книге
			{
				printf("Такой ID уже есть в телефонной книге!\n");
				printf("Введите ID: ");
				scanf_s("%d", &id);
				count_id = 0;
				i = 0;
			}
			else
			{
				count_id++;
			}
		}

		if (count_id == arr_size) // ID нет в книге 
		{
			return id;
		}
	}
}

void Enter_Man(char* line, const char* data)
{
	printf("Введите %s (напишите 0, если хотите не вносить данные): ", data);
	scanf_s("%50s", line, LINE_SIZE);

	if (data == "фамилию" || data == "имя") // Проверка на NOT NULL фамилии и имени
	{
		while (line[0] == '0')
		{
			printf("Это поле не может быть пустым!\n");
			printf("Заполните %s: ", data);
			scanf_s("%50s", line, LINE_SIZE);
		}
	}

	line = (line[0] == '0') ? line = "" : line; // Если решили оставить поле пустым
}

void Enter_Contact(char contact_arr[N][LINE_SIZE], const char* data)
{
	int count_line = 0;

	printf("Введите %s (напишите 0, если хотите не вносить данные): ", data);

	while (count_line < N)
	{
		// Вводим новую строку и копируем её содержимое в поле структуры
		char arr[LINE_SIZE] = { "" };
		scanf_s("%50s", arr, (unsigned)_countof(arr));
		strncpy_s(contact_arr[count_line], LINE_SIZE, arr, _TRUNCATE);
		count_line++;

		if (arr[0] == '0' && arr[1] == '\0') // Если решили не заполнять
		{
			break;
		}
	}
}

int Find(int num_id)
{
	int index_id = -1; // Номер строки массива

	for (int i = 0; i < arr_size; i++) // Ищем нужный номер строки с помощью ID
	{
		if (phone_book[i].man.id == num_id) // Ищем наш ID во всей книге
		{
			printf("ID '%d' найден в телефонной книге!\n", num_id);
			index_id = i;
		}
	}

	if (index_id == -1) return -1; // Ошибка!

	return index_id;
}

int Redact(int fd, int index)
{
	if (phone_book[0].man.first_name[0] == '\0') // Проверка на заполненность телефонной книги
	{
		return -1;
	}

	int choice = 0;

	// Изначальные и новые значения полей
	int old_id = phone_book[index].man.id, new_id;
	char old_line[LINE_SIZE], new_line[LINE_SIZE];
	int res;

	printf("\nВведённый ID содержит:");
	Print_Line(index);
	printf("\n\nКакое поле, хотите изменить?\n1 - ID\n2 - Фамилия\n3 - Имя\n4 - Отчество\n5 - Место работы\n6 - Должность\n");
	printf("7 - Номера телефонов\n8 - Адреса почт\n9 - Ссылки на страницы в соцсетях\n10 - Отмена\n");
	printf("Введите номер: ");
	scanf_s("%d", &choice);

	switch(choice)
	{
		case 1:
			// ID
			printf("Изменение ID\n");
			phone_book[index].man.id = Enter_ID();
			new_id = phone_book[index].man.id;
			res = 1;
			break;
			
		case 2:
			// Фамилия
			printf("Изменение фамилии\n");
			strncpy_s(old_line, LINE_SIZE, phone_book[index].man.first_name, _TRUNCATE);
			Enter_Man(phone_book[index].man.first_name, "новую фамилию");
			strncpy_s(new_line, LINE_SIZE, phone_book[index].man.first_name, _TRUNCATE);
			res = Check_Redact(old_line, new_line);
			break;

		case 3:
			// Имя
			printf("Изменение имени\n");
			strncpy_s(old_line, LINE_SIZE, phone_book[index].man.second_name, _TRUNCATE);
			Enter_Man(phone_book[index].man.second_name, "новое имя");
			strncpy_s(new_line, LINE_SIZE, phone_book[index].man.second_name, _TRUNCATE);
			res = Check_Redact(old_line, new_line);
			break;

		case 4:
			// Отчество
			printf("Изменение отчества\n");
			strncpy_s(old_line, LINE_SIZE, phone_book[index].man.third_name, _TRUNCATE);
			Enter_Man(phone_book[index].man.third_name, "новое отчество");
			strncpy_s(new_line, LINE_SIZE, phone_book[index].man.third_name, _TRUNCATE);
			res = Check_Redact(old_line, new_line);
			break;

		case 5:
			// Место работы
			printf("Изменение места работы\n");
			strncpy_s(old_line, LINE_SIZE, phone_book[index].man.place_of_work, _TRUNCATE);
			Enter_Man(phone_book[index].man.place_of_work, "новое место работы");
			strncpy_s(new_line, LINE_SIZE, phone_book[index].man.place_of_work, _TRUNCATE);
			res = Check_Redact(old_line, new_line);
			break;

		case 6:
			// Должность
			printf("Изменение должности\n");
			strncpy_s(old_line, LINE_SIZE, phone_book[index].man.post, _TRUNCATE);
			Enter_Man(phone_book[index].man.post, "новую должность");
			strncpy_s(new_line, LINE_SIZE, phone_book[index].man.post, _TRUNCATE);
			res = Check_Redact(old_line, new_line);
			break;

		case 7:
			// Номера телефонов
			printf("Изменение номеров телефонов\n");
			strncpy_s(old_line, LINE_SIZE, phone_book[index].contact.phone_number, _TRUNCATE);
			Enter_Contact(phone_book[index].contact.phone_number, "новые номера телефонов");
			strncpy_s(new_line, LINE_SIZE, phone_book[index].contact.phone_number, _TRUNCATE);
			res = Check_Redact(old_line, new_line);
			break;

		case 8:
			// Адреса почт
			printf("Изменение адресов почт\n");
			strncpy_s(old_line, LINE_SIZE, phone_book[index].contact.email, _TRUNCATE);
			Enter_Contact(phone_book[index].contact.email, "новые адреса почт");
			strncpy_s(new_line, LINE_SIZE, phone_book[index].contact.email, _TRUNCATE);
			res = Check_Redact(old_line, new_line);
			break;

		case 9:
			// Ссылки на страницы в соцсетях
			printf("Изменение ссылок на страницы в соцсетях\n");
			strncpy_s(old_line, LINE_SIZE, phone_book[index].contact.references, _TRUNCATE);
			Enter_Contact(phone_book[index].contact.references, "новые ссылки на страницы в соцсетях");
			strncpy_s(new_line, LINE_SIZE, phone_book[index].contact.references, _TRUNCATE);
			res = Check_Redact(old_line, new_line);
			break;

		case 10:
			// Отмена
			printf("Редактирование отменено\n");
			return 1;

		default:
			// Неверный ввод
			printf("Некорректный ввод\n");

			while (choice < 1 || choice > 11) // Вводим, пока не будет корректно
			{
				printf("Введите номер: ");
				scanf_s("%d", &choice);
			}

			break;
	}

	if (res)
	{

		// После успешного редактирования записываем всю таблицу обратно в файл
		if (lseek(fd, 0, SEEK_SET) < 0)
		{
			perror("Ошибка установки позиции файла");
			return -1;
		}

		if (write(fd, phone_book, sizeof(Phone_Book) * arr_size) < 0)
		{
			perror("Ошибка записи файла");
			return -1;
		}
	}

	return res;
}

int Check_Redact(char* old_line, char* new_line)
{
	for (int i = 0; i < LINE_SIZE; i++) // Сравниваем каждый символ
	{
		if (old_line[i] != new_line[i])
		{
			printf("Успешное редактирование!\n");
			return 1;
		}
	}
	
	return -1;
}

int Delete(int fd, int index)
{
	for (int i = index; i < arr_size; i++) // Удаляем запись в телефонной книге
	{
		if ((i + 1) == N) return -1; // Ошибка!
		phone_book[i] = phone_book[i + 1];
	}

	printf("Успешное удаление!\n");
	arr_size -= 1;

	close(fd);
	remove("file_phone_book");
	fd = open("file_phone_book", O_CREAT | O_RDWR);

	if (write(fd, phone_book, sizeof(Phone_Book) * arr_size) < 0)
	{
		perror("Ошибка записи в файл");
		return -1;
	}

	return arr_size;
}

void Print()
{
	if (arr_size == 0) // Проверка на заполненность телефонной книги
	{
		printf("\nВ телефонной книге ещё нет записей\n");
		return;
	}

	for (int i = 0; i < arr_size; i++) // Печатаем содержимое телефонной книги
	{
		if (i != 0) printf("\n"); // Лишняя пустая строка для красивого вывода
		Print_Line(i); // Печать одной строки
	}

	printf("\n");
}

void Print_Line(int i)
{
	printf("\n%d. %s %s\n", phone_book[i].man.id, phone_book[i].man.first_name, phone_book[i].man.second_name);
	printf("Отчество: %s\n", phone_book[i].man.third_name);
	printf("Место работы: %s\n", phone_book[i].man.place_of_work);
	printf("Должность: %s\n", phone_book[i].man.post);
	Print_Contact(phone_book[i].contact.phone_number, "Номера телефонов: ");
	Print_Contact(phone_book[i].contact.email, "\nАдреса почт: ");
	Print_Contact(phone_book[i].contact.references, "\nСсылки на страницы в соцсетях: ");
}

void Print_Contact(char contact_arr[N][LINE_SIZE], const char* data)
{
	printf("%s", data);

	for (int j = 0; j < N; j++) // Печать содержимого поля contact
	{
		if (contact_arr[j][0] == '0' || contact_arr[j][1] == '\0') // Проверка есть ли дальше информация в массиве
		{
			break;
		}

		printf("\n");

		for (int k = 0; k < LINE_SIZE; k++) // Вывод строки массива
		{
			printf("%c", contact_arr[j][k]);

			if (contact_arr[j][k] == NULL) // Проверка закончилась ли строка массива
			{
				break;
			}
		}
	}
}
