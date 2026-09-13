#include <ApplicationServices/ApplicationServices.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
int main(int argc,char **argv){
    if(argc<3)return 1;
    CGPoint p=CGPointMake(atof(argv[1]),atof(argv[2]));
    CGEventRef d=CGEventCreateMouseEvent(NULL,kCGEventLeftMouseDown,p,kCGMouseButtonLeft);
    if(d){CGEventPost(kCGHIDEventTap,d);CFRelease(d);}
    usleep(90000);
    CGEventRef u=CGEventCreateMouseEvent(NULL,kCGEventLeftMouseUp,p,kCGMouseButtonLeft);
    if(u){CGEventPost(kCGHIDEventTap,u);CFRelease(u);}
    return 0;
}
