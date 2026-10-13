#import "../../app/TouchLayoutStore.h"
#include <cassert>

int main(int argc,char **argv){
    @autoreleasepool {
        assert(argc==3);
        NSURL *url=[NSURL fileURLWithPath:[NSString stringWithUTF8String:argv[2]]];
        NSUserDefaults *defaults=[[NSUserDefaults alloc] initWithSuiteName:@"cod4.touch.persistence.test"];
        NSMutableArray *layout=[NSMutableArray array];
        for(int i=0;i<13;++i)[layout addObject:@[@(.03*i),@(.9-.02*i),@(1+.04*i)]];
        if(!strcmp(argv[1],"write-sp")){
            NSError *error=nil;
            assert([KISTouchLayoutStore saveLayout:layout opacity:.65 atURL:url defaults:defaults error:&error]);
        }else if(!strcmp(argv[1],"read-mp")){
            // Fresh process and preference instance: both engines read the same file.
            NSDictionary *saved=[KISTouchLayoutStore loadAtURL:url defaults:defaults];
            assert([saved[@"layout"] isEqual:layout]);assert(fabs([saved[@"opacity"] floatValue]-.65)<.001);
        }else if(!strcmp(argv[1],"guards")){
            NSDictionary *saved=[KISTouchLayoutStore loadAtURL:url defaults:defaults];
            assert(![KISTouchLayoutStore saveLayout:@[] opacity:1 atURL:url defaults:defaults error:nil]);
            assert([[KISTouchLayoutStore loadAtURL:url defaults:defaults] isEqual:saved]);
            NSURL *blocked=[url URLByAppendingPathComponent:@"blocked.plist"];
            assert(![KISTouchLayoutStore saveLayout:layout opacity:1 atURL:blocked defaults:defaults error:nil]);
            assert([[KISTouchLayoutStore loadAtURL:url defaults:defaults] isEqual:saved]);
            [defaults setObject:layout forKey:@"cod4.touch.layout.v1"];
            [defaults setFloat:.65 forKey:@"cod4.touch.opacity.v1"];
            [NSFileManager.defaultManager removeItemAtURL:url error:nil];
            assert([[KISTouchLayoutStore loadAtURL:url defaults:defaults][@"layout"] isEqual:layout]);
        }else assert(false);
        [defaults removePersistentDomainForName:@"cod4.touch.persistence.test"];
    }
}
