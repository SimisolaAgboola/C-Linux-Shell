#include <iostream>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <vector>
#include <string>
#include <fcntl.h>

#include "Tokenizer.h"

// all the basic colours for a shell prompt
#define RED     "\033[1;31m"
#define GREEN	"\033[1;32m"
#define YELLOW  "\033[1;33m"
#define BLUE	"\033[1;34m"
#define WHITE	"\033[1;37m"
#define NC      "\033[0m"

using namespace std;

int main () {
    vector<string> pastdirectories;
    vector<int> bgcmds;
    //create copies of stdin/stdout; dup()
	// int stdin = dup(0);
	// int stdout = dup(1);
    for (;;) {
        //check for background processes in loop
        for(string::size_type i = 0; i < bgcmds.size(); i++){
            if(waitpid(bgcmds[i], 0, WNOHANG) == bgcmds[i]){ 
                cout << "Process: " <<bgcmds[i] << " ended" <<endl;
                //remove the process from list of bg processes
                bgcmds.erase(bgcmds.begin() +i);
                i--; 
            }
        }
        // need date/time, username, and absolute path to current dir
        //getenv("USER"), time()+ctime(), getcwd();
        char* username = getenv("USER");
        //printf("\nUSER: @%s", username);
        char cwd[1024];
        time_t t = time(NULL);
        string date_time=ctime(&t);
        getcwd(cwd, sizeof(cwd));
        //printf("\nDir: %s\n", cwd);
        cout << YELLOW << username << " "<< date_time.substr(0,date_time.size()-1)<<  " " << getcwd(cwd, sizeof(cwd)) << NC << " ";
        
        // get user inputted command
        string input;
        getline(cin, input);

        if (input == "exit") {  // print exit message and break out of infinite loop
            cout << RED << "Now exiting shell..." << endl << "Goodbye" << NC << endl;
            break;
        }

        //implement cd with chdir()
        //if dir (cd <dir>) is "-", then go to previous working directory
        //variable storing previous working directory, declare outside loop
        

        // get tokenized commands from user input
        Tokenizer tknr(input);
        if (tknr.hasError()) {  // continue to next prompt if input had an error
            continue;
        }

        // // print out every command token-by-token on individual lines
        // // prints to cerr to avoid influencing autograder
        for (auto cmd : tknr.commands) {
            for (auto str : cmd->args) {
                cerr << "|" << str << "| ";
            }
            if (cmd->hasInput()) {
                cerr << "in< " << cmd->in_file << " ";
            }
            if (cmd->hasOutput()) {
                cerr << "out> " << cmd->out_file << " ";
            }
            cerr << endl;
        }

        //for piping
        //check if 1 command (no piping) or multiple
        if(tknr.commands.size()==1){
            if(tknr.commands.at(0)->args.at(0)=="cd"){
                char pathofdir[500];
                string currdir = getcwd(pathofdir, sizeof(pathofdir));
                if(tknr.commands.at(0)->args.at(1)=="-"){
                    if(pastdirectories.size()>0){
                        chdir(pastdirectories.back().c_str());
                        pastdirectories.push_back(currdir);
                    }
                }
                else{
                    pastdirectories.push_back(currdir);
                    chdir(tknr.commands.at(0)->args.at(1).c_str());
                }
                continue;
            }
            // fork to create child
            pid_t pid;
            pid = fork();

            if (pid < 0) {  // error check
                perror("fork");
                exit(2);
            }
            //add check for bg process -add pid to vector if bg and don't waitpid() in parent

            if (pid == 0) {  // if child, exec to run command
                // check for single commands with no arguments
                //char* args[] = {(char*) tknr.commands.at(0)->args.at(0).c_str(), nullptr};
                char** args = new char*[(tknr.commands.at(0)->args.size()) + 1];
                for (size_t i = 0; i < tknr.commands.at(0)->args.size(); i++){
                    args[i] = (char*) tknr.commands.at(0)->args.at(i).c_str();
                }
                args[tknr.commands.at(0)->args.size()] = nullptr;
                
                //if current command is redirected, then open file and dup2 std(in/out) that's being redirected
                //implement it for both at same time
                if(tknr.commands.at(0)->hasInput()){
                    int fd=open(tknr.commands.at(0)->in_file.c_str(), O_RDONLY|O_CREAT,0644);
                    dup2(fd,0);
                }
                if(tknr.commands.at(0)->hasOutput()){
                    int fd=open(tknr.commands.at(0)->out_file.c_str(),O_WRONLY|O_CREAT|O_TRUNC,0644);
                    dup2(fd,1);
                }
                
                if (execvp(args[0], args) < 0) {  // error check
                    perror("execvp");
                    exit(2);
                }
            }
            else {  // if parent, wait for child to finish
                if(tknr.commands[0]->isBackground()){
                        bgcmds.push_back(pid);   
                }
                else{
                    int status = 0;
                    waitpid(pid, &status, 0);
                    if (status > 1) {  // exit if child didn't exec properly
                        exit(status);
                    }
                }
            }
               
            //restore stdin/stdout (variable will be outside the loop)

            
            //else{}//piping}

            //for cmd: commands
            //call pipe() to make pipe
            // fork() in child, redirect stdout, in parent- redirect stdin
            //add checks for first/last command
            
            
        }
        else{//piping 
            pid_t p1;

            //loop thru commands vector
            for(size_t i=0; i<tknr.commands.size(); i++){
                //create pipe
                int fds[2];
                if(i<tknr.commands.size()-1){ //not last command, then create pipe
                    if(pipe(fds)<0){
                        perror("pipe");
                        exit(2);
                    }
                }
                p1 = fork();
                if (p1 < 0) {  // error check
                    perror("fork");
                    exit(2);
                }
                if(p1==0){ //child
                    
                    Command* cmd=tknr.commands[i];
                    if(i==tknr.commands.size()-1){//if this  is the last command
                        if(cmd->hasOutput()){//check for output redirection
                            int fd=open(cmd->out_file.c_str(),O_WRONLY|O_CREAT|O_TRUNC,0644);
                            dup2(fd,1);
                        }
                    }
                    else{
                        if(i==0){
                            if(cmd->hasInput()){ //check for input redirection in 1st cmd
                                int fd=open(cmd->in_file.c_str(), O_RDONLY|O_CREAT,0644);
                                dup2(fd,0);
                            }
                        }
                        dup2(fds[1],1); //redirect stdout into write
                        close(fds[0]); //close read
                    } 
                    char** args = new char*[(tknr.commands.at(i)->args.size()) + 1];
                    for (size_t j = 0; j < tknr.commands.at(j)->args.size(); j++){
                        args[j] = (char*) tknr.commands.at(i)->args.at(i).c_str();
                    }
                    args[tknr.commands.at(0)->args.size()] = nullptr;
                    

                    if (execvp(args[0], args) < 0) {  // error check
                        perror("execvp");
                        exit(2);
                    }
                }
                else{
                    if(i<tknr.commands.size()-1){
                        close(fds[1]); //close write
                        dup2(fds[0],0); //redirect stdin into read
                        close(fds[0]);
                    }
                }  
                if(tknr.commands[0]->isBackground()){
                        bgcmds.push_back(p1);   
                }
                else{
                    int status = 0;
                    waitpid(p1, &status, 0);
                    if (status > 1) {  // exit if child didn't exec properly
                        exit(status);
                    }
                }              

            }
        }
    } 
}
