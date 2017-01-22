/**
*  @file arduino_serial.c Example of communication with Arduino in Linux.
*  @author Andrey Sharoyko
*  @created 2012/09/18
*  @seealso http:///sites.google.com/site/vanyambauseslinux/
*/
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>
#include <cstring>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <iostream>

/*#include <linux/delay.h> */
     
#define ACC 	'A'+'C'+'C'
#define VB 	'V'+'B'
#define ON 	'O'+'N'
#define OFF 	'O'+'F'+'F'
#define UP	'U'+'P'
#define DWN	'D'+'W'+'N'
#define SET	'S'+'E'+'T'
#define WIFI	'W'+'I'+'F'+'I'
#define CELL	'C'+'E'+'L'+'L'
#define USB	'U'+'S'+'B'
#define DBG	'D'+'B'+'G'


    FILE *fp_pid;
    FILE *fp_msg;							// Указатель на файл сообщений от сервера
    FILE *fp_msg_play;							// Указатель на файл синхронизатора 
    const char pid_f[] = "/var/run/ioa.pid";				/* Файл PID */    
    const char wrk_d[] = "/tmp/cpc-cb";					/* Рабочий каталог */    
    const char cmsg_f1[] = "/messages";					// Название файла сообщений от сервера    
    const char cmsg_f2[] = "/messages-play";				// Название файла сообщений от локального синхронизатора syncplay   
    const char cacc_f[] = "/acc";					/* Название файла для контроля ACC */
    const char cvb_f[] = "/vb";						/* Название файла для контроля Vbatt */

    char msg_f1[256];							// Файл сообщений от сервера    
    char msg_f2[256];							// Файл сообщений от локального синхронизатора syncplay   
    char acc_f[256];							/* Файл для контроля ACC */
    char vb_f[256];							/* Файл для контроля Vbatt */


    int fd;								// Дескриптор файла устройства контроллера
    int res;								// Возвращаемое значение
    int time_acc;							// Частота обновления сигнала ACC и напряжения батареи
    int old_acc;							// сохраненное значение сигнала ACC

    bool dbg = true;							// Флаг отладки
    bool wrk=true;							// Флаг окончания работы и выгрузки драйвера



    sig_atomic_t shoot = 0;
//    sig_atomic_t shoot_sig=0;

void read_stat(int);							// Объявление обработчика команд
void sendCommand(int, const char *);
void cln_exit(int);
void I_O_ard (int,char *);

/*
* Signal handler
*/
void hdl(int signo)
{
    shoot=1;
//    shoot_sig=signo;
//    if (dbg) printf("\n--------------------------------------------\n");
//    if (signo == SIGUSR1) {
//        if (dbg) printf("Полученный сигнал:		SIGUSR1\n");				// Приняли сигнал от сервера
//        read_stat(signo); }
//    else if (signo == SIGUSR2) {
//	if (dbg) printf("Полученный сигнал:		SIGUSR2\n");				// Приняли сигнал от синхронизатора
//        read_stat(signo); }
//    else
//    {
//	if (dbg) printf("Полученный сигнал:		SIGTERM\n");
//	wrk=false;										// Закансиваем работать
//    }
//    if (dbg) printf("Вышли из обработчика событий\n");
}

void cln_exit(int signo)
{
    if (dbg) printf("Чистимся... Сигнал:		%u\n", signo);
    sendCommand(fd,"f\r");			/* Отключаем USB от хоста */
    sleep(1);
    sendCommand(fd,"c 0\r");
    sleep(1);
    sendCommand(fd,"w 0\r");
    sleep(1);
    sendCommand(fd,"u 0\r");
    unlink(msg_f1);
    unlink(msg_f2);
    unlink(acc_f);
    unlink(vb_f);
    rmdir(wrk_d);
    unlink(pid_f);
    close(fd);
    if (dbg) printf("Все почистили за собой\n");
}
/**
*  Get Arduino device file name from command line arguments.
*/
char* getDeviceFileName(int argc, char **argv)
{
    if (argc < 2)
    {
    fprintf(stderr, "Argument missing: Arduino device file.\n");
    exit(EXIT_FAILURE);
    }
    return argv[1];
}

/**
*  Open Arduino device file.
*/
int openDeviceFile(const char* fileName, const char* mode)
{
    FILE* file;

    file = fopen(fileName, mode);
    if (file == NULL) {
    perror(fileName);
    exit(EXIT_FAILURE);
    }
    return fileno(file);
}

/**
*  Get baud rate from command line arguments.
*/
//unsigned int getBaudRate(int argc, char **argv)
//{
//    return atoi(globalArgs.portSpeed);
//    return (argc > 2) ? atoi(argv[2]) : 57600;
//}

/**
*  Set Arduino serial port attributes.
*/
void setAttr(int fd, unsigned int baud)
{
    struct termios toptions;
    speed_t brate;

    if (tcgetattr(fd, &toptions) < 0) {
        perror("Can't get term attributes");
        exit(EXIT_FAILURE);
    }

    switch(baud) {
	case 4800:   brate=B4800;   break;
        case 9600:   brate=B9600;   break;
    #ifdef B14400
        case 14400:  brate=B14400;  break;
    #endif
        case 19200:  brate=B19200;  break;
    #ifdef B28800
	case 28800:  brate=B28800;  break;
    #endif
	case 38400:  brate=B38400;  break;
	case 57600:  brate=B57600;  break;
        case 115200: brate=B115200; break;
        case 230400: brate=B230400; break;
        default:
            fprintf(stderr, "Invalid baud rate value: %d\n", baud);
    	    exit(EXIT_FAILURE);
    }
    cfsetispeed(&toptions, brate);
    cfsetospeed(&toptions, brate);

     // 8N1
    toptions.c_cflag &= ~PARENB;
    toptions.c_cflag &= ~CSTOPB;
    toptions.c_cflag &= ~CSIZE;
    toptions.c_cflag |= CS8;
    // no flow control
    toptions.c_cflag &= ~CRTSCTS;

    toptions.c_cflag |= CREAD | CLOCAL;  // turn on READ & ignore ctrl lines
    toptions.c_iflag &= ~(IXON | IXOFF | IXANY); // turn off s/w flow ctrl

    toptions.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); // make raw
    toptions.c_oflag &= ~OPOST; // make raw

     // see: http://unixwiz.net/techtips/termios-vmin-vtime.html
    toptions.c_cc[VMIN]  = 0;
    toptions.c_cc[VTIME] = 20;

    if (tcsetattr(fd, TCSANOW, &toptions) < 0) {
        perror("Can't set term attributes");
        exit(EXIT_FAILURE);
    }
}

/**
*  Send command to Arduino.
*/
void sendCommand(int fd, const char *cmd)
{
    write(fd, cmd, strlen(cmd));
}

/**
*  Display command result from Arduino.
*/

int displayResult(int fd, char otv[])
{
    char buf[256]={0};
    ssize_t sz;

    sz = read(fd, buf, 255);					// Прочитали в буфер и вернули количество прочтенных байт
    buf[sz-2] = 0;						// Записали конец строки
    if (dbg)
    {
	printf("Получен ответ контроллера:	%s\n", buf);
	printf("Количество символов ответа:	%u\n",sz);
    }
    for (int i=0; i<=(sz-2); i++) otv[i]=buf[i]; 
}


void read_stat(int signo)
{
    FILE *fp;
    
    char str[25]={0};
    
    if (signo == SIGUSR1)
    {
	if((fp=fopen(msg_f1, "r"))==NULL) 
	{
	    printf("Не удается открыть файл.\n");
    	    exit(1);
	}
	else if (dbg) printf("Файл команды:			%s.\n",::msg_f1);	    
    }
    else if (signo == SIGUSR2)
    {
	if((fp=fopen(msg_f2, "r"))==NULL) 
	{
	    printf("Не удается открыть файл.\n");
    	    exit(1);
	}
	else if (dbg) printf("Файл команды:			%s.\n", ::msg_f2);	    

    }

    while(!feof(fp)) {                          	          /* Читаем файл статуса светодиодов */
	(fgets(str,25, fp)); if (dbg) printf("Прочитали команду:		%s\n", str);  /* Запоминаем в массиве и печатаем */
    }

    fclose(fp);                                        		   /* Закрыли файл статуса светодиодов */


// Вычисляем хэш статуса сигнала светодиодов
    int comd=0;
    int i;
    int lens = 0;
    char *div;
    size_t maxlen;
    size_t lens_t;
    lens=strnlen(str,25);
    lens_t=strlen(str);
    if (dbg) printf ("Количество символов запроса:	%d\n",lens);
    div = strchr(str, ' ');
    if (div == 0)
	{
	if (dbg) printf("Команда без аргументов:		");
	for (i=0;i<=lens; i++) {comd = comd + str[i]; printf("%c",str[i]);}
	if (dbg) printf(" (хеш команды: %d)\n", comd);
	}
    else
	{
	if (dbg) printf("Команда с аргументом:		");
	for (i=0;i<lens; i++)
	    {
		if (str[i] != ' ')
		{
		    comd = comd + str[i];
		    if (dbg) printf("%c",str[i]);
		}
	    else break;
	    }
	div=div+1;							// Установили позицию аргумента (указатель на подстроку)
	if (dbg) printf(" (хеш команды: %d), аргумент: %s\n", comd,div);
	}
    I_O_ard(comd,div);

// Чистим файлы по прочтении команды
    if (signo == SIGUSR1)
	{fp=fopen(msg_f1, "w"); fclose(fp);}
    else if (signo == SIGUSR2)
	{fp=fopen(msg_f2, "w"); fclose(fp);}

}


void I_O_ard (int SIG,char *SIG_arg)
{
    FILE *fp_acc;
    FILE *fp_vb;
    char ret[50]={0};
    char tm[80]={0};							/* Строка для команды */
    const char endmsg[] ="\r";						/* Признак понца команды */

    switch ( SIG )
	{
	case ON:	sendCommand(fd,"o\r"); break;			/* Подключаем USB к хосту */
	case OFF:	sendCommand(fd,"f\r"); break;			/* Отключаем USB от хоста */
	case UP:	sendCommand(fd,"h\r"); break;			/* Сервер стартанул */
	case DWN:							/* Сервер остановлен */
		strcat(tm,"z ");					/* Команда засыпания */
		strcat(tm,SIG_arg);						/* Интервал времени для сна */
		strcat(tm,endmsg);					/* Записываем конец сообщения */
	    	sendCommand(fd, tm);					/* Посылаем команду */
	break;
	case ACC:
	    {							/* Проверяем статус ACC */
		sendCommand(fd,"v\r");					/* Посылаем команду */
		sleep(1);
		res = displayResult(fd,ret);
		if (ret[0] > '0' && old_acc == 0)
		{
		    old_acc == 1;
		    fp_acc=fopen(acc_f,"w");
		    fprintf(fp_acc,"ON");
		    fclose(fp_acc);
		    if (dbg) printf("Статус сигнала:			ON\n");
		}
		else if (ret[0] < '1' && old_acc == 1)
		{
		    old_acc=0;
		    unlink(acc_f);
		    if (dbg) printf("Статус сигнала:			OFF\n");
		}


//		if (ret[0] > '0') {fp_acc=fopen(acc_f,"w");fprintf(fp_acc,"ON"); fclose(fp_acc);} else unlink(acc_f);
//		fp_acc=fopen(acc_f,"w");
//		if (ret[0] > '0') fprintf(fp_acc,"ON"); else fprintf(fp_acc,"OFF");
//		fclose(fp_acc);
//		alarm(time_acc);
	    }
	break;

	case VB:							/* Получаем напряжение батареи */
	    {
		sendCommand(fd,"b\r");					/* Посылаем команду */
		sleep(1);
		res = displayResult(fd,ret);				/* Получаем ответ */
		fp_vb=fopen(vb_f,"w");
		fprintf(fp_vb,"%s",ret);
		fclose(fp_vb);
	    }
	break;
	
	case WIFI:							/* Индикатор WiFi */
	    {
		strcat(tm,"w ");					/* Команда индикации WiFi */
		strcat(tm,SIG_arg);						/* Режим индикации */
		strcat(tm,endmsg);					/* Формируем конец сообщения*/
	    	sendCommand(fd, tm);					/* Посылаем команду */
	    }
	break;
	case CELL:							/* Индикатор сотовой связи */
	    {
		strcat(tm,"c ");					/* Команда индикации */
		strcat(tm,SIG_arg);					/* Режим индикации */
		strcat(tm,endmsg);
	    	sendCommand(fd, tm);					/* Посылаем команду*/
	    }
	break;
	case USB:							/* Индикатор USB */
	    {
		strcat(tm,"u ");					/* Команда индикации */
		strcat(tm,SIG_arg);					/* Режим индикации */
		strcat(tm,endmsg);					/* Записываем конец сообщения */
	    	sendCommand(fd, tm);					/* Посылаем команду */
	    }
	break;
	case DBG:							/* Режим отладки */
	    {
		strcat(tm,"x ");					/* Команда отладки */
		strcat(tm,SIG_arg);					/* Режим отладки */
		strcat(tm,endmsg);					/* Записываем конец сообщения */
	    	sendCommand(fd, tm);					/* Посылаем команду */
	    }
	break;
    }
}



/**
*  Main function.
*/
int main(int argc, char *argv[])
{
//using namespace std;

    char *fileName;
    const char fileMode[] = "r+";
    unsigned int baud;
    int shoot_sig;
    FILE *fp_acc;
    
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = hdl;
    sigset_t   set;
    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    sigaddset(&set, SIGUSR2);
    sigaddset(&set, SIGTERM);
    sigaddset(&set, SIGALRM);
    act.sa_mask = set;


    sigprocmask(SIG_BLOCK, &set, 0);					// Заблокировались на время инициализации

	int opt = 0;
    struct globalArgs_t {
	bool noDebug;							/* параметр -dbg */
	char *portContr;             					/* параметр -p */
	char *portSpeed;						/* параметр -s */
//    FILE *outFile;
//    int verbosity;              /* параметр -v */
//    char **inputFiles;          /* входные файлы */
//    int numInputFiles;          /* число входных файлов */
    } globalArgs;

    static const char *optString = "dp:s:h?";
    
    /* инициализация globalArgs до начала работы с ней. */
    globalArgs.noDebug = 0;     /* false */
    globalArgs.portContr = NULL;
    globalArgs.portSpeed = NULL;
//    globalArgs.outFile = NULL;
//    globalArgs.verbosity = 0;
//    globalArgs.inputFiles = NULL;
//    globalArgs.numInputFiles = 0;

    opt = getopt( argc, argv, optString );
	while( opt != -1 ) {
    	    switch( opt ) {
        	case 'd':
            	    globalArgs.noDebug = 1; /* true */
                break;
                
        	case 'p':
            	    globalArgs.portContr = optarg;
                break;
                
        	case 's':
            	    globalArgs.portSpeed = optarg;
                break;
                
//        	case 'v':
//            	    globalArgs.verbosity++;
//                break;
                
//        	case 'h':   /* намеренный проход в следующий case-блок */
//		case '?':
//            	    display_usage();
//                break;
                
        	default:
            	    /* сюда на самом деле попасть невозможно. */
		break;
    	    }
        
        opt = getopt( argc, argv, optString );
    }
    
//    globalArgs.inputFiles = argv + optind;
//    globalArgs.numInputFiles = argc - optind;

    dbg = globalArgs.noDebug;
    fileName = globalArgs.portContr;   


//    mode_t process_mask = umask(0);					// Права доступа
    int result_code = mkdir(wrk_d, S_IRWXU | S_IRWXG | S_IRWXO);	// создаем рабочий каталог
//    umask(process_mask);						// Возвращаем права доступа процессу
    fp_pid = fopen(pid_f,"w");
    fprintf(fp_pid, "%i", getpid());					// Записали ПИД
    fclose(fp_pid);

// Формируем имена рабочих файлов
    strcpy(msg_f1,wrk_d);
    strncat(msg_f1,cmsg_f1,strlen(cmsg_f1));
    strcpy(msg_f2,wrk_d);
    strncat(msg_f2,cmsg_f2,strlen(cmsg_f2));
    strcpy(acc_f,wrk_d);
    strncat(acc_f,cacc_f,strlen(cacc_f));
    strcpy(vb_f,wrk_d);
    strncat(vb_f,cvb_f,strlen(cvb_f));
// Создаем файлы сообщений    
    fp_msg = fopen(msg_f1,"w");						// Создали файл сообщений для сервера 
    fclose(fp_msg);
    fp_msg_play = fopen(msg_f2,"w");					// Создали файл сообщений для синхронизатора
    fclose(fp_msg_play);

// Открываем com-порт, настраиваем

//    fileName = getDeviceFileName(argc, argv);
    fd = openDeviceFile(fileName, fileMode);
    baud = atoi(globalArgs.portSpeed);
//    baud = getBaudRate(argc, argv);
    setAttr(fd, baud);
    sendCommand(fd,"\r");								/* Чистим приемный буфер ардуино */

    I_O_ard(ACC,0);								// Установили начальное значение ACC и взвели таймер
    I_O_ard(VB,0);
    alarm(10);

    if (dbg) printf ("PID is : %i\n", getpid());
    if (dbg) printf("ON=%d OFF=%d UP=%d DWN=%d ACC=%d VB=%d WIFI=%d CELL=%d USB=%d DBG=%d\n\n",ON,OFF,UP,DWN,ACC,VB,WIFI,CELL,USB,DBG);
    
    while(wrk)
	{
	    sigwait(&set,&shoot_sig);						// Спим, ждем сигнала
		if (dbg) printf("\n--------------------------------------------\n");
		if (shoot_sig == SIGALRM)
		    {
		    if (dbg) printf("Полученный сигнал:		SIGALRM\n");				// Приняли сигнал от сервера
		    I_O_ard(ACC,0);
		    I_O_ard(VB,0);
		    alarm(10);
		    }
		else if (shoot_sig == SIGTERM)
		    {
		    if (dbg) printf("Полученный сигнал:		SIGTERM\n");				// Приняли сигнал от сервера
		    wrk=false;
		    }
		else
		    {
		    if (dbg) printf("Полученный сигнал SIGUSR:	%u\n",shoot_sig);				// Приняли сигнал от сервера
		    read_stat(shoot_sig);
		    }
		if (dbg) {std::cout << "Вышли из обработчика\n"; std::cout<<"----------------------------------------------\n";}
	}

/*******
* Выход с чисткой
*******/
    cln_exit(SIGTERM);
    close(fd);
    return EXIT_SUCCESS;
}
                                                                                                                                                                                                                                                                                                                                                          