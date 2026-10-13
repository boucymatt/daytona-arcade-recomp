/* Isolated RSX textured geometry test. All resources live until process exit. */
#include <string.h>
#include <math.h>
#include "geometry_batch.h"
#include "texture_decode.h"
#include "layer_compare.h"
#include "tile_packet.h"
#include "tile_spans.h"
#include "tile_colour.h"
#include <rsx/rsx_program.h>
#include "probe_shaders.h"
static rsxVertexProgram *probe_vp;
static rsxFragmentProgram *probe_fp;
static void *probe_vcode;
static u32 probe_foffset, probe_voffset;
static int probe_pos, probe_uv, probe_sampler;
static gcmTexture probe_texture;
#ifdef PS3_RSX_GAME
static float *probe_game_vertices;
static unsigned probe_game_count;
static const Ps3GeometryBatch *probe_game_batches;
static unsigned probe_game_batch_count;
static int probe_mapping, probe_bounds, probe_modes;
static u32 *probe_game_texture;
static uint64_t probe_texture_generation;
static int probe_texture_valid;
static unsigned probe_uploaded_bytes;
static gcmTexture probe_shades;
static u32 *probe_shade_pixels;
static int probe_shade_sampler;
static gcmTexture probe_layers;
static u32 *probe_layer_pixels;
static int probe_layer_sampler;
static int probe_tiledata_sampler;
static gcmTexture probe_tiledata;
static u32 *probe_tiledata_pixels;
static rsxFragmentProgram *layer_fp;
static u32 layer_foffset;
static unsigned layer_rows_uploaded;
static unsigned layer_spans_rebuilt;
static int upload_game_layers(const Ps3TilePacket *tiles) {
    u32 converted[4][512] __attribute__((aligned(16)));
    static u32 cached_pens[4096];
    static u32 colours[2][4096][4];
    static int pens_valid;
    int palette_changed=!pens_valid || memcmp(cached_pens,tiles->pens,sizeof cached_pens);
    if(palette_changed) {
        memcpy(cached_pens,tiles->pens,sizeof cached_pens); pens_valid=1;
        for(unsigned category=0;category<2;++category)
            for(unsigned p=0;p<4096;++p)
                for(unsigned variant=0;variant<4;++variant)
                    colours[category][p][variant]=ps3_tile_colour(p,category,variant,cached_pens[p]);
    }
    layer_rows_uploaded = 0;
    for (unsigned layer=0; layer<4; ++layer) for (unsigned y=0; y<512; ++y) {
        if (!palette_changed && !(tiles->rows[layer] & ((uint64_t)1 << (y/8)))) continue;
        // Decode each source pixel once, not once per transparency variant.
        // Palette-dependent branching is amortized over the cached lookup.
        for (unsigned x=0; x<512; ++x) {
            unsigned i=y*512+x, p=tiles->pixels[layer][i]&4095;
            const u32 *colour=colours[tiles->flags[layer][i]&1][p];
            converted[0][x]=colour[0]; converted[1][x]=colour[1];
            converted[2][x]=colour[2]; converted[3][x]=colour[3];
        }
        for(unsigned variant=0;variant<4;++variant)
            memcpy(probe_layer_pixels+(variant*512+y)*2048+layer*512,converted[variant],sizeof converted[variant]);
        ++layer_rows_uploaded;
    }
    // The colour shader reads only pen zero from this texture. Registers and
    // palette lookups for tile pixels have already moved to the CPU cache.
    if(palette_changed) probe_tiledata_pixels[0]=cached_pens[0]|0xff000000u;
    static uint16_t cached_words[12288];
    static int spans_valid;
    layer_spans_rebuilt=0;
    if(spans_valid && !memcmp(cached_words,tiles->words,sizeof cached_words)) return 1;
    if(!prepare_tile_spans(tiles)) return 0;
    static float span_vertices[PS3_TILE_VERTICES*20];
    static Ps3TileRect cached_rects[PS3_TILE_RECTS];
    static unsigned cached_rect_count;
    unsigned changed_first=tile_rect_count, changed_end=0;
    const unsigned corners[6] = {0,1,2,0,2,3};
    for (unsigned i=0; i<tile_rect_count; ++i) {
        if(i<cached_rect_count && !memcmp(&cached_rects[i],&tile_rects[i],sizeof(Ps3TileRect))) continue;
        if(i<changed_first) changed_first=i;
        changed_end=i+1;
        cached_rects[i]=tile_rects[i];
        for (unsigned j=0; j<6; ++j) {
        const Ps3TileRect *r=&tile_rects[i];
        unsigned c = corners[j];
        float dx=(c==1 || c==2)?r->w:0,dy=c>=2?r->h:0;
        float *v = span_vertices + (i*6+j)*20;
        memset(v, 0, 20*sizeof(float));
        v[0]=(r->x+dx)/248.f-1.f; v[1]=1.f-(r->y+dy)/192.f; v[3]=1;
        int variant=r->flags==3?3:r->flags==1?2:r->category;
        v[8]=(r->layer*512+r->sx+dx)/2048.f; v[9]=(variant*512+r->sy+dy)/2048.f;
        v[10]=r->category; v[11]=r->flags;
        }
    }
    // Unchanged rectangles retain their CPU and RSX vertices. Keep draw counts
    // current even when the list shrinks; stale tail vertices are never drawn.
    if(changed_end>changed_first)
        memcpy(probe_game_vertices+131072*20+changed_first*120,
               span_vertices+changed_first*120,(changed_end-changed_first)*120*sizeof(float));
    cached_rect_count=tile_rect_count;
    memcpy(cached_words,tiles->words,sizeof cached_words);
    spans_valid=1;
    layer_spans_rebuilt=1;
    return 1;
}
/* Called only after the previous flip has completed: never overwrite in-flight data. */
static int texture_game_upload(const void *a, const void *b, uint64_t generation) {
    if (!a || !b) return 0;
    if (probe_texture_valid && generation == probe_texture_generation) return 1;
    static unsigned char cached_sheets[2][2097152];
    u32 decoded[2][1024] __attribute__((aligned(16)));
    unsigned changed_pairs=0;
    for (unsigned sheet=0; sheet<2; ++sheet) {
        const unsigned char *src = sheet ? b : a;
        for (unsigned y=0; y<4096; y+=2) {
            const unsigned offset=(y/2)*1024;
            if(probe_texture_valid && !memcmp(cached_sheets[sheet]+offset,src+offset,1024)) continue;
            for(unsigned row=0;row<2;++row) {
                for (unsigned x=0; x<1024; ++x) {
                    unsigned n = ps3_texture_index(src, x, y+row) * 17u;
                    decoded[row][x] = 0xff000000u | n*0x010101u;
                }
                // Stage in CPU memory, then transfer contiguous rows to RSX.
                memcpy(probe_game_texture+(y+row)*2048+sheet*1024,decoded[row],sizeof decoded[row]);
            }
            memcpy(cached_sheets[sheet]+offset,src+offset,1024);
            ++changed_pairs;
        }
    }
    probe_uploaded_bytes=changed_pairs*8192;
    probe_texture_generation = generation; probe_texture_valid = 1;
    return 1;
}
#endif
static int texture_probe_init(void) {
    probe_vp = (rsxVertexProgram *)probe_v;
    probe_fp = (rsxFragmentProgram *)probe_f;
    void *fcode; u32 vsize, fsize;
    rsxVertexProgramGetUCode(probe_vp, &probe_vcode, &vsize);
    rsxFragmentProgramGetUCode(probe_fp, &fcode, &fsize);
    rsxProgramAttrib *position = rsxVertexProgramGetAttrib(probe_vp, "position");
    rsxProgramAttrib *uv = rsxVertexProgramGetAttrib(probe_vp, "uv");
#ifdef PS3_RSX_GAME
    rsxProgramAttrib *mapping = rsxVertexProgramGetAttrib(probe_vp, "mapping");
    rsxProgramAttrib *bounds = rsxVertexProgramGetAttrib(probe_vp, "bounds");
    rsxProgramAttrib *modes = rsxVertexProgramGetAttrib(probe_vp, "modes");
    rsxProgramAttrib *image = rsxFragmentProgramGetAttrib(probe_fp, "image");
    rsxProgramAttrib *shades = rsxFragmentProgramGetAttrib(probe_fp, "shades");
    if (!shades) return 0;
    probe_shade_sampler=shades->index;
    layer_fp = (rsxFragmentProgram *)layer_f;
    rsxProgramAttrib *layers = rsxFragmentProgramGetAttrib(layer_fp, "layers");
    rsxProgramAttrib *data = rsxFragmentProgramGetAttrib(layer_fp, "tiledata");
    if (!position || !uv || !mapping || !bounds || !modes || !image || !layers || !data) return 0;
    probe_tiledata_sampler = data->index;
    probe_layer_sampler = layers->index;
    probe_bounds = bounds->index; probe_modes = modes->index;
    probe_mapping = mapping->index;
    probe_pos = position->index; probe_uv = uv->index; probe_sampler = image->index;
#else
    rsxProgramAttrib *image = rsxFragmentProgramGetAttrib(probe_fp, "image");
    if (!position || !uv || !image) return 0;
    probe_pos = position->index; probe_uv = uv->index; probe_sampler = image->index;
#endif
    if (!vsize || !fsize || probe_pos < 0 || probe_uv < 0 || probe_sampler < 0) return 0;
    void *fragment = rsxMemalign(64, fsize);
    float *vertices = rsxMemalign(128,
#ifdef PS3_RSX_GAME
        (131072+PS3_TILE_VERTICES) * 20 * sizeof(float)
#else
        9 * 6 * sizeof(float)
#endif
    );
#ifdef PS3_RSX_GAME
    probe_game_vertices = vertices;
#endif
    u32 *texture = rsxMemalign(128,
#ifdef PS3_RSX_GAME
        2048 * 4096 * 4
#else
        64 * 64 * 4
#endif
    );
    if (!fragment || !vertices || !texture) return 0;
    memcpy(fragment, fcode, fsize);
    const float quad[54] = {
        -.65f,-.65f,.5f,1, 0,1,  .65f,-.65f,.5f,1, 1,1,  .65f,.65f,.5f,1, 1,0,
        -.65f,-.65f,.5f,1, 0,1,  .65f,.65f,.5f,1, 1,0,  -.65f,.65f,.5f,1, 0,0,
        // Near triangle samples a single orange texel. Draw order must not
        // change its visibility over the more distant checkerboard.
        -.45f,-.4f,-.5f,1,.18f,.05f, .45f,-.4f,-.5f,1,.18f,.05f,
         0,.5f,-.5f,1,.18f,.05f};
    memcpy(vertices, quad, sizeof quad);
    for (unsigned y=0; y<64; ++y) for (unsigned x=0; x<64; ++x)
        texture[y*64+x] = ((x/8) ^ (y/8)) & 1 ? 0xffff8000 : 0xff00c0ff;
    if (rsxAddressToOffset(fragment, &probe_foffset) || rsxAddressToOffset(vertices, &probe_voffset)
        || rsxAddressToOffset(texture, &probe_texture.offset)) return 0;
    probe_texture.format = GCM_TEXTURE_FORMAT_A8R8G8B8 | GCM_TEXTURE_FORMAT_LIN;
    probe_texture.mipmap = 1; probe_texture.dimension = GCM_TEXTURE_DIMS_2D;
    probe_texture.width = 64; probe_texture.height = 64; probe_texture.depth = 1;
    probe_texture.pitch = 256; probe_texture.location = GCM_LOCATION_RSX;
#ifdef PS3_RSX_GAME
    probe_game_texture = texture;
    probe_texture.width = 2048; probe_texture.height = 4096; probe_texture.pitch = 8192;
#endif
    probe_texture.remap = (GCM_TEXTURE_REMAP_TYPE_REMAP << GCM_TEXTURE_REMAP_TYPE_B_SHIFT)
        | (GCM_TEXTURE_REMAP_TYPE_REMAP << GCM_TEXTURE_REMAP_TYPE_G_SHIFT)
        | (GCM_TEXTURE_REMAP_TYPE_REMAP << GCM_TEXTURE_REMAP_TYPE_R_SHIFT)
        | (GCM_TEXTURE_REMAP_TYPE_REMAP << GCM_TEXTURE_REMAP_TYPE_A_SHIFT)
        | (GCM_TEXTURE_REMAP_COLOR_B << GCM_TEXTURE_REMAP_COLOR_B_SHIFT)
        | (GCM_TEXTURE_REMAP_COLOR_G << GCM_TEXTURE_REMAP_COLOR_G_SHIFT)
        | (GCM_TEXTURE_REMAP_COLOR_R << GCM_TEXTURE_REMAP_COLOR_R_SHIFT)
        | (GCM_TEXTURE_REMAP_COLOR_A << GCM_TEXTURE_REMAP_COLOR_A_SHIFT);
    __asm__ volatile("sync" ::: "memory");
#ifdef PS3_RSX_GAME
    probe_layer_pixels = rsxMemalign(128, 2048*2048*4);
    probe_shade_pixels=rsxMemalign(128,256*4096*4);
    if(!probe_shade_pixels) return 0;
    probe_shades=probe_texture;
    probe_shades.width=256; probe_shades.height=4096; probe_shades.pitch=1024;
    if(rsxAddressToOffset(probe_shade_pixels,&probe_shades.offset)) return 0;
    if (!probe_layer_pixels) return 0;
    memset(probe_layer_pixels, 0, 2048*2048*4);
    probe_layers = probe_texture;
    probe_layers.width = 2048; probe_layers.height = 2048; probe_layers.pitch = 8192;
    if (rsxAddressToOffset(probe_layer_pixels, &probe_layers.offset)) return 0;
    probe_tiledata_pixels = rsxMemalign(128, 256*64*4);
    if (!probe_tiledata_pixels) return 0;
    probe_tiledata = probe_layers;
    probe_tiledata.width = 256; probe_tiledata.height = 64; probe_tiledata.pitch = 1024;
    if (rsxAddressToOffset(probe_tiledata_pixels, &probe_tiledata.offset)) return 0;
    void *layer_code; u32 layer_size;
    rsxFragmentProgramGetUCode(layer_fp, &layer_code, &layer_size);
    void *layer_memory = rsxMemalign(64, layer_size);
    if (!layer_size || !layer_memory) return 0;
    memcpy(layer_memory, layer_code, layer_size);
    if (rsxAddressToOffset(layer_memory, &layer_foffset)) return 0;
    __asm__ volatile("sync" ::: "memory");
#endif
    return 1;
}
static void texture_probe_draw(gcmContextData *ctx, unsigned width, unsigned height, unsigned frame) {
    float scale[4] = {width*.5f, height*-.5f, .5f, 0};
    float offset[4] = {width*.5f, height*.5f, .5f, 0};
#ifdef PS3_RSX_GAME
    // Preserve the original 4:3 display aspect inside the selected video mode.
    float content_width = height * (4.f / 3.f);
    float content_height = height;
    if (content_width > width) { content_width = width; content_height = width * .75f; }
    scale[0] = content_width * .5f; scale[1] = content_height * -.5f;
#endif
    rsxSetViewport(ctx, 0, 0, width, height, 0, 1, scale, offset);
    rsxSetCullFaceEnable(ctx, GCM_FALSE);
    rsxSetDepthTestEnable(ctx, GCM_TRUE);
    rsxSetDepthWriteEnable(ctx, GCM_TRUE);
    rsxSetDepthFunc(ctx, GCM_LESS);
    rsxSetBlendEnable(ctx, GCM_FALSE);
    rsxLoadVertexProgram(ctx, probe_vp, probe_vcode);
    rsxLoadFragmentProgramLocation(ctx, probe_fp, probe_foffset, GCM_LOCATION_RSX);
#ifdef PS3_RSX_GAME
    rsxBindVertexArrayAttrib(ctx, probe_pos, 0, probe_voffset, 80, 4, GCM_VERTEX_DATA_TYPE_F32, GCM_LOCATION_RSX);
    rsxBindVertexArrayAttrib(ctx, probe_uv, 0, probe_voffset+16, 80, 4, GCM_VERTEX_DATA_TYPE_F32, GCM_LOCATION_RSX);
    rsxBindVertexArrayAttrib(ctx, probe_mapping, 0, probe_voffset+32, 80, 4, GCM_VERTEX_DATA_TYPE_F32, GCM_LOCATION_RSX);
    rsxBindVertexArrayAttrib(ctx, probe_bounds, 0, probe_voffset+48, 80, 4, GCM_VERTEX_DATA_TYPE_F32, GCM_LOCATION_RSX);
    rsxBindVertexArrayAttrib(ctx, probe_modes, 0, probe_voffset+64, 80, 4, GCM_VERTEX_DATA_TYPE_F32, GCM_LOCATION_RSX);
#else
    rsxBindVertexArrayAttrib(ctx, probe_pos, 0, probe_voffset, 24, 4, GCM_VERTEX_DATA_TYPE_F32, GCM_LOCATION_RSX);
    rsxBindVertexArrayAttrib(ctx, probe_uv, 0, probe_voffset+16, 24, 2, GCM_VERTEX_DATA_TYPE_F32, GCM_LOCATION_RSX);
#endif
    rsxInvalidateTextureCache(ctx, GCM_INVALIDATE_TEXTURE);
    rsxLoadTexture(ctx, probe_sampler, &probe_texture);
    rsxTextureControl(ctx, probe_sampler, GCM_TRUE, 0, 0, GCM_TEXTURE_MAX_ANISO_1);
    rsxTextureFilter(ctx, probe_sampler, 0, GCM_TEXTURE_NEAREST, GCM_TEXTURE_NEAREST, GCM_TEXTURE_CONVOLUTION_QUINCUNX);
    rsxTextureWrapMode(ctx, probe_sampler, GCM_TEXTURE_CLAMP_TO_EDGE, GCM_TEXTURE_CLAMP_TO_EDGE,
                      GCM_TEXTURE_CLAMP_TO_EDGE, 0, GCM_TEXTURE_ZFUNC_LESS, 0);
#ifdef PS3_RSX_GAME
    rsxLoadFragmentProgramLocation(ctx, layer_fp, layer_foffset, GCM_LOCATION_RSX);
    rsxLoadTexture(ctx, probe_layer_sampler, &probe_layers);
    rsxLoadTexture(ctx, probe_tiledata_sampler, &probe_tiledata);
    rsxTextureControl(ctx, probe_tiledata_sampler, GCM_TRUE, 0, 0, GCM_TEXTURE_MAX_ANISO_1);
    rsxTextureFilter(ctx, probe_tiledata_sampler, 0, GCM_TEXTURE_NEAREST, GCM_TEXTURE_NEAREST, GCM_TEXTURE_CONVOLUTION_QUINCUNX);
    rsxTextureWrapMode(ctx, probe_tiledata_sampler, GCM_TEXTURE_CLAMP_TO_EDGE, GCM_TEXTURE_CLAMP_TO_EDGE,
                      GCM_TEXTURE_CLAMP_TO_EDGE, 0, GCM_TEXTURE_ZFUNC_LESS, 0);
    rsxTextureControl(ctx, probe_layer_sampler, GCM_TRUE, 0, 0, GCM_TEXTURE_MAX_ANISO_1);
    rsxTextureFilter(ctx, probe_layer_sampler, 0, GCM_TEXTURE_NEAREST, GCM_TEXTURE_NEAREST, GCM_TEXTURE_CONVOLUTION_QUINCUNX);
    rsxTextureWrapMode(ctx, probe_layer_sampler, GCM_TEXTURE_CLAMP_TO_EDGE, GCM_TEXTURE_CLAMP_TO_EDGE,
                      GCM_TEXTURE_CLAMP_TO_EDGE, 0, GCM_TEXTURE_ZFUNC_LESS, 0);
    rsxSetDepthTestEnable(ctx, GCM_FALSE); rsxSetDepthWriteEnable(ctx, GCM_FALSE);
    if(tile_front_first) rsxDrawVertexArray(ctx, GCM_TYPE_TRIANGLES, 131072, tile_front_first*6);
    rsxLoadFragmentProgramLocation(ctx, probe_fp, probe_foffset, GCM_LOCATION_RSX);
    rsxLoadTexture(ctx, probe_sampler, &probe_texture);
    rsxSetDepthTestEnable(ctx, GCM_TRUE); rsxSetDepthWriteEnable(ctx, GCM_TRUE);
    rsxLoadTexture(ctx,probe_shade_sampler,&probe_shades);
    rsxTextureControl(ctx,probe_shade_sampler,GCM_TRUE,0,0,GCM_TEXTURE_MAX_ANISO_1);
    rsxTextureFilter(ctx,probe_shade_sampler,0,GCM_TEXTURE_NEAREST,GCM_TEXTURE_NEAREST,GCM_TEXTURE_CONVOLUTION_QUINCUNX);
    rsxTextureWrapMode(ctx,probe_shade_sampler,GCM_TEXTURE_CLAMP_TO_EDGE,GCM_TEXTURE_CLAMP_TO_EDGE,GCM_TEXTURE_CLAMP_TO_EDGE,0,GCM_TEXTURE_ZFUNC_LESS,0);
    for (unsigned i = 0; i < probe_game_batch_count; ++i) {
        const Ps3GeometryBatch *b = &probe_game_batches[i];
        const float left = (width - content_width)*.5f, top = (height - content_height)*.5f;
        // Pixel centres inside transformed half-open clip edges.
        int x0 = (int)ceilf(left + b->x0*content_width/496.f - .5f);
        int x1 = (int)ceilf(left + b->x1*content_width/496.f - .5f);
        int y0 = (int)ceilf(top + b->y0*content_height/384.f - .5f);
        int y1 = (int)ceilf(top + b->y1*content_height/384.f - .5f);
        if (x0 < 0) x0 = 0;
        if (y0 < 0) y0 = 0;
        if (x1 > (int)width) x1 = width;
        if (y1 > (int)height) y1 = height;
        if (x1 <= x0 || y1 <= y0) continue;
        rsxSetScissor(ctx, x0, y0, x1-x0, y1-y0);
        rsxDrawVertexArray(ctx, GCM_TYPE_TRIANGLES, b->first, b->count);
    }
    rsxSetScissor(ctx, 0, 0, width, height);
    rsxLoadFragmentProgramLocation(ctx, layer_fp, layer_foffset, GCM_LOCATION_RSX);
    rsxLoadTexture(ctx, probe_layer_sampler, &probe_layers);
    rsxLoadTexture(ctx, probe_tiledata_sampler, &probe_tiledata);
    rsxSetDepthTestEnable(ctx, GCM_FALSE); rsxSetDepthWriteEnable(ctx, GCM_FALSE);
    if(tile_rect_count>tile_front_first)
        rsxDrawVertexArray(ctx, GCM_TYPE_TRIANGLES, 131072+tile_front_first*6, (tile_rect_count-tile_front_first)*6);
#else
    if ((frame / 60) & 1) {
        rsxDrawVertexArray(ctx, GCM_TYPE_TRIANGLES, 6, 3);
        rsxDrawVertexArray(ctx, GCM_TYPE_TRIANGLES, 0, 6);
    } else {
        rsxDrawVertexArray(ctx, GCM_TYPE_TRIANGLES, 0, 6);
        rsxDrawVertexArray(ctx, GCM_TYPE_TRIANGLES, 6, 3);
    }
#endif
}
