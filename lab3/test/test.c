#include<stdio.h>

int main(){
    char filename[10];
    char c;
    for(int i=1;i<32;i++){
        sprintf(filename,"test%d",i);
        freopen(filename,"w",stdout);
        if(i<=10){
            c=i-1+'0';
        }
        else{
            c=i-11+'a';
        }
        for(long long j=0; j<1048576;j++){
            printf("%c",c);
        }
        printf("\n");
        printf("This is the end of %s. \n",filename);
    }
    return 0;
}