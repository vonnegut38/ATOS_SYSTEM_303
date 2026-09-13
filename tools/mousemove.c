#include <ApplicationServices/ApplicationServices.h>
#include <stdlib.h>
#include <stdio.h>
int main(int argc,char **argv){
    if(argc<3){fprintf(stderr,"usage: mousemove x y\n");return 1;}
    CGPoint p=CGPointMake(atof(argv[1]),atof(argv[2]));
    CGEventRef e=CGEventCreateMouseEvent(NULL,kCGEventMouseMoved,p,kCGMouseButtonLeft);
    if(!e)return 2;
    CGEventPost(kCGHIDEventTap,e);
    CFRelease(e);
    return 0;
}
