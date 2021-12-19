//2017116248 Lee, Jaejoon

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <ncursesw/curses.h>
#include <ncurses.h>
#include <string.h>
#include <aio.h>
#include <termios.h>
#include <fcntl.h>
#include <ctype.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/select.h>
#include <pthread.h>
#include <semaphore.h>
#include <locale.h>

#define LEFT 15
#define RIGHT 115
#define ESC 27

int input = -1; //press any key
char message[] = "321"; //count down instead of saying Korean
int pos = 110; //position
char person1[] = "^O^"; //not died
char person2[] = "T_T"; // died
char sullae[] = "______\n    /      \\\n   | ______ |\n   | =    = |\n   |    <   |\n   \\___-___/\n      |  |\n    /      \\\n    |      |\n";
char sullae2[] = "______\n    /      \\\n   | ______ |\n   | +    + |\n   |    >   |\n   \\___O___/\n      |  |\n    /      \\\n    |      |\n";
char line[] = "              |\n              |\n              |\n              |\n              |\n";
int stage = 1; // 1~5
int time_to_kill =0; //1->ready to shot
int end = 0;
int success = 0;
int ismoved = 0; //1 -> move

void printMainScreen();
void printSecondScreen();
void printBackground();
void printGameFrame();
int get_response( char *, int );
void set_cr_noecho_mode(void);
void set_nodelay_mode(void);
void tty_mode(int);
void * printCount();
void * printShot();
void * moving();
void * printSullae();
void quithandler(int sig);
// void delay(clock_t n);

int main(int argc, char *argv[]){
    int pid;
    pthread_t t1;
    int status;
    // setlocale(LC_CTYPE,"ko_KR.utf-8"); 
    clock_t start_time, end_time;
    float time;
    char char_time[10];
    int fds[2]; // for IPC (pipe)
    struct sigaction sa;
    struct itimerval timer;

	signal( SIGINT,  SIG_IGN ); //ignore SIGINT
	signal( SIGQUIT, SIG_IGN ); //ignore SIGQUIT
    memset (&sa, 0, sizeof (sa)); // timer signal for quit
    sa.sa_handler = &quithandler;
    sigaction (SIGALRM, &sa, NULL);
    // signal( SIGALRM, quithandler );

	tty_mode(0);				/* save tty mode	*/
	set_cr_noecho_mode();			/* set -icanon, -echo	*/

    system("clear");
    printMainScreen();
    
    input = getchar(); //press any key
    if(input == ESC){
        system("clear");
        tty_mode(1);				/* restore tty mode	*/
        return 0;
    }else{
        system("clear");
        move(0,0);
        printSecondScreen();
    }

    //curses
    initscr();

    //curses color
    start_color();
    init_pair(1, COLOR_RED, COLOR_BLACK);
    init_pair(2, COLOR_BLUE, COLOR_BLACK);
    init_pair(3, COLOR_GREEN, COLOR_BLACK);
    init_pair(4, COLOR_YELLOW, COLOR_BLACK);
    init_pair(5, COLOR_CYAN, COLOR_BLACK);
    init_pair(6, COLOR_MAGENTA, COLOR_BLACK);
    init_pair(7, COLOR_WHITE, COLOR_BLACK);
    
    pipe(fds);

    pid = fork();
    if(pid == -1){ //fork error
        perror("fork error");
        exit(1);
    }

    while(1){
        if(stage > 5){ // if ended last stage
            break;
        }

        //game start

        if(pid == 0){  //child
            
            tty_mode(0);				/* save tty mode	*/
            set_cr_noecho_mode();			/* set -icanon, -echo	*/

            signal( SIGQUIT,  quithandler ); //only quit, no sigint
            
            pthread_create(&t1, NULL, printCount, NULL);
            pthread_detach(t1);
            
            system("clear");
            clear();
            move(0,0);
            start_time = clock();

            while(1){
                if(ismoved){ //game done
                    for(int i=LEFT+15; i<= RIGHT; i = i+10){
                        //background
                        printBackground();

                        //player is catched
                        mvaddstr(5, 5, sullae2);
                        attron(COLOR_PAIR(1));
                        mvaddstr(20, 50, "You died.... lol");
                        attroff(COLOR_PAIR(1));
                        refresh();

                        if(i >= pos){
                            end = 1; //game end
                            break;
                        }
                        
                        //arrow
                        attron(COLOR_PAIR(4));
                        mvaddstr(10, i, "==>");
                        attroff(COLOR_PAIR(4));
                        attron(COLOR_PAIR(2));
                        mvaddstr(10,pos, person2);
                        attroff(COLOR_PAIR(2));
                        refresh();
                        sleep(1);
                        mvaddstr(10, i, "   ");
                        refresh();
                        attron(COLOR_PAIR(4));
                        mvaddstr(10, i+5, "==>");
                        attroff(COLOR_PAIR(4));
                        attron(COLOR_PAIR(2));
                        mvaddstr(10,pos, person2);
                        attroff(COLOR_PAIR(2));
                        refresh();
                    }

                    success = 0;

                    system("clear");
                    return 0; // game done
                }

                //background
                printBackground();

                //bottom line
                printGameFrame();
                
                //user
                mvaddstr(10, pos, person1);
                refresh();

                input = getch();
                if(input == 'D' || input == 'd') { //move right
                    if(pos < RIGHT){
                        mvaddstr(10,pos, "   ");
                        pos = pos + 1;
                        //refresh();
                    }
                    if(time_to_kill){
                        ismoved = 1;
                        break;
                    }
                }
                if(input == 'A' || input == 'a') { //move left
                    if(pos > LEFT){
                        mvaddstr(10,pos, "   ");
                        pos = pos - 1;
                        //refresh();
                    }
                    if(time_to_kill){
                        ismoved = 1;
                        break;
                    }
                }

                if(pos < LEFT+3){ // stage clear
                    stage = stage + 1;

                    end_time = clock();
                    time = (float)(end_time - start_time)/CLOCKS_PER_SEC; //get collaped time

                    system("clear");
                    clear();
                    
                    write(fds[1], &time, sizeof(time)); // pipe write

                    return stage; // to parent
                }

                if(time_to_kill == 0){ //game playing
                    mvaddstr(5, 5, sullae);
                }else{//player is catched
                    mvaddstr(5, 5, sullae2);
                }
                
                refresh();
            }
        } // end child
        else{ //parent
            wait(&status); //wait for child's return value

            if(status != 0){ //success
                read(fds[0], &time, sizeof(time)); //read from pipe

                if(stage >= 5){ //player wins
                    alarm(4);
                    while(1){
                        attron(COLOR_PAIR(4));
                        mvaddstr(9, 55, "Congrats!!");
                        mvaddstr(10, 40, "You are the owner of $45,600,000 !!!");
                        refresh();
                        attroff(COLOR_PAIR(4));
                        usleep(500000);

                        attron(COLOR_PAIR(3));
                        mvaddstr(9, 55, "Congrats!!");
                        refresh();
                        attroff(COLOR_PAIR(3));
                        usleep(500000);

                        attron(COLOR_PAIR(2));
                        mvaddstr(9, 55, "Congrats!!");
                        refresh();
                        attroff(COLOR_PAIR(2));
                        usleep(500000);

                        attron(COLOR_PAIR(1));
                        mvaddstr(9, 55, "Congrats!!");
                        refresh();
                        attroff(COLOR_PAIR(1));
                        usleep(500000);
                    }

                    //below -> just in case
                    system("clear");
                    endwin();
                    tty_mode(1); //restore

                    return 0;
                }else{ //clear one stage
                    printf("\n\n\n\n\n                                              collapsed time : %.3f sec.\n", time*1000); 
                    printf("                                                                                        stage %d clear, go to next stage!\n\n\n", stage);
                }
                stage ++; //next stage
                sleep(3);
                clear();

                pid = fork(); //child is died. need to fork again (in while loop)
            }
            else{ //player losed
                attroff(COLOR_PAIR(4));
                refresh();

                alarm(3);
                while(1){
                    attron(COLOR_PAIR(4));
                    mvaddstr(10, 50, "Game over T.T");
                    refresh();
                    usleep(500000);
                    mvaddstr(10, 50, "             ");
                    refresh();
                    mvaddstr(10, 50, "Game over T^T");
                    refresh();
                    usleep(500000);
                    attroff(COLOR_PAIR(4));
                }

                //below -> just in case
                system("clear");
                endwin();
                tty_mode(1);				/* restore tty mode	*/

                return 0;
            }
            
            continue;
        } //end parent
    }

    //below -> just in case
    printf("game over unnormally\n");
    sleep(1);
    
    system("clear");
    endwin();
    tty_mode(1);				/* restore tty mode	*/

    return 0;
}

void printBackground(){
    attron(COLOR_PAIR(7));
    mvaddstr(1, 50, "T^T");
    attroff(COLOR_PAIR(7));
    attron(COLOR_PAIR(2));
    mvaddstr(3, 70, "T.T");
    attroff(COLOR_PAIR(2));
    attron(COLOR_PAIR(3));
    mvaddstr(7, 44, "T-T");
    attroff(COLOR_PAIR(3));
    attron(COLOR_PAIR(4));
    mvaddstr(17, 33, "T.T");
    attroff(COLOR_PAIR(4));
    attron(COLOR_PAIR(5));
    mvaddstr(15, 83, "T^T");
    attroff(COLOR_PAIR(5));
    attron(COLOR_PAIR(6));
    mvaddstr(14, 59, "T-T");
    attroff(COLOR_PAIR(6));
    attron(COLOR_PAIR(1));
    mvaddstr(2, 28, "T_T");
    attroff(COLOR_PAIR(1));
    attron(COLOR_PAIR(5));
    mvaddstr(4, 39, "T+T");
    attroff(COLOR_PAIR(5));
}

void quithandler(int sig){ // accept only quit

    system("clear");
    endwin();
    tty_mode(1);				/* restore tty mode	*/
    
    exit(0);
}

void printGameFrame(){
    attron(COLOR_PAIR(5));
    mvaddstr(20, 0, ":::::::::::::::::::");
    attroff(COLOR_PAIR(5));
    attron(COLOR_PAIR(3));
    mvaddstr(20,19, "#");
    attroff(COLOR_PAIR(3));
    mvaddstr(20,20, ":::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::");
    mvaddstr(5, 5, sullae);
}

void * printCount(){
    time_to_kill = 0;
    
    while(time_to_kill == 0 && ismoved == 0){
        for(int i = 0; i < 3; i ++){
            printBackground();

            printGameFrame();

            attron(COLOR_PAIR(4));
            mvaddch(9, 20, message[i]); 
            attroff(COLOR_PAIR(4)); 
            refresh(); 
            mvaddstr(10,pos, person1);    
            refresh();

            usleep(1000000 - 120000*(stage-1)); // will be harder at higher level
        }

        //count over
        refresh();
        mvaddstr(5, 5, sullae2); 
        refresh();
        mvaddstr(10,pos, person1);  
        refresh();
        attron(COLOR_PAIR(1));
        mvaddstr(9, 20, "STOP!");
        attroff(COLOR_PAIR(1));
        refresh();

        //don't move
        time_to_kill = 1; // 
        usleep(1000000 - 120000*(stage-1)); // will be harder at higher level

        //player can move agian
        time_to_kill = 0;
        clear();
    }
}

//start page
void printMainScreen(){
    printf("\n\n\n");
    printf("                  ######   #######    ##   ##  ##  #####     ######      #      ##     ##  ######\n");
    printf("                 #        ##     ##   ##   ##  ##  ##  ##   ##          # #     ###    ##  ##\n");
    printf("                  ######  ##     ##   ##   ##  ##  ##   ##  ##  ####   #####    #### ####  ######\n");
    printf("                       #  ##    ###   ##   ##  ##  ##  ##   ##    ##  ##   ##   ##  #  ##  ##\n");
    printf("                 ######    ##### ###  #######  ##  #####     ######  ##     ##  ##     ##  ######\n\n");
    printf("                ==================================================================================\n");
    printf("                =======================오징어 게임 : 무궁화 꽃이 피었습니다=======================\n");
    printf("                =============================2021FALL SYSTEM PROGRAMMING==========================\n");
    printf("                =================================2017116248 이재준================================\n");
    printf("                ==================================================================================\n\n\n\n\n");
    printf("                                            < PRESS ANY KEY TO START >\n");
    printf("                                 If you want to exit this game, please press ESC.\n\n\n");
}

//second page
void printSecondScreen(){
    for(int i=5;i>=0;i--){ //count down 5
        printf("\n\n\n");
        printf("                                    오징어 게임의 “무궁화 꽃이 피었습니다.” 게임입니다.\n");
        printf("                                    술래가 뒤돌아 있는 동안 결승점을 향해 달리면 됩니다.\n");
        printf("                                     술래와 눈을 마주친 동안 움직이게 되면 탈락입니다.\n");
        printf("                             왼쪽으로 움직이려면 A를, 오른쪽으로 움직이려면 D를 누르면 됩니다.\n");
        printf("                                                귀하의 건승을 기원합니다.\n\n\n");
        if(i != 0){
            printf("                                                            %d !\n", i);
        }else{
            printf("                                                       GAME START !\n");
        }
        sleep(1);
        system("clear");
    }
}

//sleep ---------> use usleep
// void delay(clock_t n){
//   clock_t start = clock();
//   while(clock() - start < n);
// }

void set_cr_noecho_mode(void){
/*
 * purpose: put file descriptor 0 into chr-by-chr mode and noecho mode
 *  method: use bits in termios
 */
	struct termios ttystate;
	
	tcgetattr( 0, &ttystate);		/* read curr.  setting	*/
	ttystate.c_lflag	&= ~ICANON;	/* no buffering		*/
	ttystate.c_lflag	&= ~ECHO;	/* no echo either	*/
	ttystate.c_cc[VMIN]	= 1;		/* get 1 char at a time */
	tcsetattr( 0, TCSANOW, &ttystate);	/* install settings 	*/
}

void set_nodelay_mode(void){
/* purose: put file descriptor 0 into no-delay mode
 *  method: use fcntl to set bits
 *  notes: tcsetattr() will do something similar, but it is complicated
 */
	int termflags;
	termflags = fcntl(0, F_GETFL);		/* read curr. settings */
	termflags |= O_NDELAY;			/* flip on nodelay bit */
	fcntl(0, F_SETFL, termflags);		/* and insall	       */
}

/* how == 0 => save current mode, how == 1 ==> restore mode */
/* this version handles termios and fcntl flags	    */
void tty_mode(int how){
	static struct termios	original_mode;
	static int	      	original_flags;
	static int 		stored = 0;

	if( how == 0 ){
		tcgetattr(0, &original_mode);
		original_flags = fcntl(0, F_GETFL);
		stored = 1;
	}
	else if( stored ){
		tcsetattr(0, TCSANOW, &original_mode);
		original_flags = fcntl(0, F_SETFL, original_flags);
	}

}

void ctrl_c_handler( int signum ){
/*
 * purpose: called if SIGINT is dectected
 * action : reset tty and scram
 */
	tty_mode(1);
	exit(1);
}