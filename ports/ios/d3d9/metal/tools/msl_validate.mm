// Offline check for the Direct3D 9 to Metal shader translator.
//
// Translates every dumped shader (Documents/shaders/*.bin from the app) and
// compiles the result with the Metal framework, the same runtime compiler the
// game uses. Writes the generated source next to a report of any failures.
//
// Build (macOS): clang++ -std=c++20 -fobjc-arc -framework Metal -framework Foundation \
//     ports/ios/d3d9/metal/dx9_msl_translator.cpp ports/ios/d3d9/metal/tools/msl_validate.mm -o work/tools/msl_validate
// Run:   work/tools/msl_validate work/shaders work/shaders_msl

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include "../dx9_msl_translator.h"

#include <cstdio>
#include <vector>

int main(int argc, const char *argv[])
{
    @autoreleasepool
    {
        if (argc < 2)
        {
            fprintf(stderr, "usage: %s <shader dump dir> [msl output dir]\n", argv[0]);
            return 2;
        }
        NSString *inputDir = [NSString stringWithUTF8String:argv[1]];
        NSString *outputDir = argc > 2 ? [NSString stringWithUTF8String:argv[2]] : nil;
        NSFileManager *files = NSFileManager.defaultManager;
        if (outputDir)
            [files createDirectoryAtPath:outputDir withIntermediateDirectories:YES attributes:nil error:nil];

        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        if (!device)
        {
            fprintf(stderr, "no Metal device\n");
            return 1;
        }
        MTLCompileOptions *options = [MTLCompileOptions new];
        options.languageVersion = MTLLanguageVersion2_4;

        NSArray<NSString *> *names = [[files contentsOfDirectoryAtPath:inputDir error:nil] sortedArrayUsingSelector:@selector(compare:)];
        int total = 0, translateFailures = 0, compileFailures = 0;
        for (NSString *name in names)
        {
            if (![name hasSuffix:@".bin"])
                continue;
            ++total;
            NSData *data = [NSData dataWithContentsOfFile:[inputDir stringByAppendingPathComponent:name]];
            std::vector<uint32_t> tokens(data.length / 4);
            memcpy(tokens.data(), data.bytes, tokens.size() * 4);

            kisak::metal::TranslatedShader shader;
            if (!kisak::metal::TranslateShader(tokens.data(), tokens.size(), {}, shader))
            {
                ++translateFailures;
                printf("TRANSLATE %s: %s\n", name.UTF8String, shader.error.c_str());
                continue;
            }
            NSString *source = [NSString stringWithUTF8String:shader.source.c_str()];
            if (outputDir)
                [source writeToFile:[outputDir stringByAppendingPathComponent:[name stringByReplacingOccurrencesOfString:@".bin" withString:@".metal"]]
                         atomically:YES encoding:NSUTF8StringEncoding error:nil];
            NSError *error = nil;
            id<MTLLibrary> library = [device newLibraryWithSource:source options:options error:&error];
            if (!library || ![library newFunctionWithName:@"kisak_main"])
            {
                ++compileFailures;
                NSString *message = error.localizedDescription ?: @"missing entry point";
                if (compileFailures <= 8)
                    printf("COMPILE %s:\n%s\n", name.UTF8String, message.UTF8String);
                else
                    printf("COMPILE %s: failed\n", name.UTF8String);
            }
        }
        printf("\n%d shaders, %d translation failures, %d compile failures\n", total, translateFailures, compileFailures);
        return (translateFailures || compileFailures) ? 1 : 0;
    }
}
