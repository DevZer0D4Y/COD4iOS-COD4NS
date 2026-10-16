#pragma once
#import <Metal/Metal.h>

namespace kisak::metal {
inline MTLCompileOptions *ShaderCompileOptions()
{
    MTLCompileOptions *options = [MTLCompileOptions new];
    options.languageVersion = MTLLanguageVersion2_4;
    // COD4 renders the same geometry with separate depth and lighting shaders.
    // Honor [[invariant]] on their position outputs so compiler reassociation
    // cannot make the lighting pass fail the existing depth comparison.
    options.preserveInvariance = YES;
    return options;
}
}
