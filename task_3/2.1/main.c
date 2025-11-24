// main
#include "PhoneBook.h"

int Open_File()
{
	int fd = open("file_phone_book", O_CREAT | O_RDWR);
	if (fd < 0) 
	{
		perror("Ошибка открытия файла");
		return -1;
	}

	// При загрузке файла читаем полностью массив контактов (максимум N штук)
	int bytes_read = read(fd, phone_book, sizeof(Phone_Book) * N);
	if (bytes_read < 0) 
	{
		perror("Ошибка чтения файла");
		close(fd);
		return -1;
	}

	// Вычисляем количество считанных контактов
	arr_size = (int)(bytes_read / sizeof(Phone_Book));
	if (phone_book[0].man.id == 0) arr_size = 0;

	// Перематываем файловый указатель в начало, чтобы можно было дозаписывать
	if (lseek(fd, 0, SEEK_SET) < 0) 
	{
		perror("Ошибка lseek");
		close(fd);
		return -1;
	}

	return fd;
}

void Menu(int fd)
{
	int choice = 0, err = 1, num_id;

	printf("\n1 - добавить\n2 - редактировать\n3 - удалить\n4 - выйти\n");
	printf("Введите команду: ");
	scanf_s("%d", &choice);

	switch (choice)
	{
		case 1:
			// Добавить
			err = Insert(fd);

			if (err == -1)
			{
				printf("Ошибка в функции добавления: недостаточно места для новой записи!\n");
			}

			break;

		case 2:
			// Редактировать
			printf("Для редактирования строки введите ID нужной строки: ");
			scanf_s("%d", &num_id);

			err = Find(num_id);

			if (err == -1)
			{
				printf("Ошибка в функции поиска: ID не найден!\n");
			}
			else
			{
				err = Redact(fd, err);

				if (err == -1)
				{
					printf("Ошибка в функции редактирования!\n");
				}
			}

			break;

		case 3:
			// Удалить
			printf("Для удаления введите ID нужной строки: ");
			scanf_s("%d", &num_id);

			err = Find(num_id);

			if (err == -1)
			{
				printf("Ошибка в функции поиска: ID не найден!\n");
			}
			else
			{
				err = Delete(fd, err);

				if (err == -1)
				{
					printf("Ошибка в функции удаления!\n");
				}
			}

			break;

		case 4:
			// Выйти
			close(fd);
			exit(0);
			break;

		default:
			// Неверный ввод
			printf("Некорректный ввод\n");
			Menu(fd);
			break;
	}
}

int main()
{
	printf("Задание 2.1: телефонная книга\n");

	int fd = Open_File();

	while (1)
	{
		Print();
		Menu(fd);
	}
}