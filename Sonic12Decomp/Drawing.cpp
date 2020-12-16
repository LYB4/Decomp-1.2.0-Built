#include "RetroEngine.hpp"

short blendLookupTable[BLENDTABLE_SIZE];
short subtractLookupTable[BLENDTABLE_SIZE];
short tintLookupTable[TINTTABLE_SIZE];

int SCREEN_XSIZE   = 424;
int SCREEN_CENTERX = 424 / 2;

DrawListEntry drawListEntries[DRAWLAYER_COUNT];

int gfxDataPosition;
GFXSurface gfxSurface[SURFACE_MAX];
byte graphicData[GFXDATA_MAX];

int InitRenderDevice()
{
    char gameTitle[0x40];

    sprintf(gameTitle, "%s%s", Engine.gameWindowText, Engine.usingDataFile ? "" : " (Using Data Folder)");

    Engine.frameBuffer = new ushort[SCREEN_XSIZE * SCREEN_YSIZE];
    memset(Engine.frameBuffer, 0, (SCREEN_XSIZE * SCREEN_YSIZE) * sizeof(ushort));

#if RETRO_USING_SDL
    SDL_Init(SDL_INIT_EVERYTHING);

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
    SDL_SetHint(SDL_HINT_RENDER_VSYNC, Engine.vsync ? "1" : "0");

    Engine.window = SDL_CreateWindow(gameTitle, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_XSIZE, SCREEN_YSIZE,
                                     SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);

    Engine.renderer = SDL_CreateRenderer(Engine.window, -1, SDL_RENDERER_ACCELERATED);

    if (!Engine.window) {
#if RSDK_DEBUG
        printf("ERROR: failed to create window!\n");
#endif
        Engine.gameMode = ENGINE_EXITGAME;
        return 0;
    }

    if (!Engine.renderer) {
#if RSDK_DEBUG
        printf("ERROR: failed to create renderer!\n");
#endif
        Engine.gameMode = ENGINE_EXITGAME;
        return 0;
    }

    SDL_RenderSetLogicalSize(Engine.renderer, SCREEN_XSIZE, SCREEN_YSIZE);
    SDL_SetRenderDrawBlendMode(Engine.renderer, SDL_BLENDMODE_BLEND);

    Engine.screenBuffer = SDL_CreateTexture(Engine.renderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, SCREEN_XSIZE, SCREEN_YSIZE);

#if RSDK_DEBUG
    if (!Engine.screenBuffer) {
        printf("ERROR: failed to create screen buffer!\nerror msg: %s\n", SDL_GetError());
        return 0;
    }
#endif

    if (Engine.fullScreen) {
        SDL_RestoreWindow(Engine.window);
        SDL_SetWindowFullscreen(Engine.window, SDL_WINDOW_FULLSCREEN_DESKTOP);
        Engine.isFullScreen = true;
    }
    else {
        SDL_SetWindowSize(Engine.window, SCREEN_XSIZE * Engine.windowScale, SCREEN_YSIZE * Engine.windowScale);
    }

    if (Engine.borderless) {
        SDL_RestoreWindow(Engine.window);
        SDL_SetWindowBordered(Engine.window, SDL_FALSE);
    }

    SDL_SetWindowResizable(Engine.window, SDL_FALSE);
    SDL_SetWindowPosition(Engine.window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
#endif

    OBJECT_BORDER_X2 = SCREEN_XSIZE + 0x80;
    // OBJECT_BORDER_Y2 = SCREEN_YSIZE + 0x100;

    return 1;
}
void RenderRenderDevice()
{
    if (Engine.gameMode == ENGINE_EXITGAME)
        return;

#if RETRO_USING_SDL
    SDL_Rect destScreenPos;
    destScreenPos.x = 0;
    destScreenPos.y = 0;
    destScreenPos.w = SCREEN_XSIZE;
    destScreenPos.h = SCREEN_YSIZE;

    int pitch = 0;
    SDL_SetRenderTarget(Engine.renderer, NULL);
    ushort *pixels = NULL;
    
    SDL_LockTexture(Engine.screenBuffer, NULL, (void **)&pixels, &pitch);
    memcpy(pixels, Engine.frameBuffer, pitch * SCREEN_YSIZE);
    SDL_UnlockTexture(Engine.screenBuffer);

    SDL_RenderCopy(Engine.renderer, Engine.screenBuffer, NULL, &destScreenPos);

    SDL_RenderPresent(Engine.renderer);
#endif
}
void ReleaseRenderDevice()
{
    if (Engine.frameBuffer)
        delete[] Engine.frameBuffer;
#if RETRO_USING_SDL
    SDL_DestroyTexture(Engine.screenBuffer);
    Engine.screenBuffer = NULL;

    SDL_DestroyRenderer(Engine.renderer);
    SDL_DestroyWindow(Engine.window);
#endif
}

void GenerateBlendLookupTable(void)
{
    int tintValue;
    int blendTableID;

    blendTableID = 0;
    for (int y = 0; y < BLENDTABLE_YSIZE; y++) {
        for (int x = 0; x < BLENDTABLE_XSIZE; x++) {
            blendLookupTable[blendTableID]      = y * x >> 8;
            subtractLookupTable[blendTableID++] = y * ((BLENDTABLE_XSIZE - 1) - x) >> 8;
        }
    }

    for (int i = 0; i < TINTTABLE_SIZE; i++) {
        tintValue = ((i & 0x1F) + ((i & 0x7E0) >> 6) + ((i & 0xF800) >> 11)) / 3 + 6;
        if (tintValue > 31)
            tintValue = 31;
        tintLookupTable[i] = 0x841 * tintValue;
    }
}

void ClearScreen(byte index)
{
    ushort colour       = activePalette[index];
    ushort *framebuffer = Engine.frameBuffer;
    int cnt             = SCREEN_XSIZE * SCREEN_YSIZE;
    while (cnt--) {
        *framebuffer = colour;
        ++framebuffer;
    }
}

void SetScreenSize(int width, int height)
{
    SCREEN_XSIZE        = width;
    SCREEN_CENTERX      = (width / 2);
    SCREEN_SCROLL_LEFT  = SCREEN_CENTERX - 8;
    SCREEN_SCROLL_RIGHT = SCREEN_CENTERX + 8;
    OBJECT_BORDER_X2    = width + 0x80;

    // SCREEN_YSIZE               = height;
    // SCREEN_CENTERY             = (height / 2);
    // SCREEN_SCROLL_UP    = (height / 2) - 8;
    // SCREEN_SCROLL_DOWN  = (height / 2) + 8;
    // OBJECT_BORDER_Y2   = height + 0x100;
}

void DrawObjectList(int Layer)
{
    int size = drawListEntries[Layer].listSize;
    for (int i = 0; i < size; ++i) {
        objectLoop = drawListEntries[Layer].entityRefs[i];
        int type           = objectEntityList[objectLoop].type;
        if (type) {
            if (scriptData[objectScriptList[type].subDraw.scriptCodePtr] > 0)
                ProcessScript(objectScriptList[type].subDraw.scriptCodePtr, objectScriptList[type].subDraw.jumpTablePtr, SUB_DRAW);
        }
    }
}
void DrawStageGFX(void)
{
    waterDrawPos = waterLevel - yScrollOffset;
    if (waterDrawPos < 0)
        waterDrawPos = 0;

    if (waterDrawPos > SCREEN_YSIZE)
        waterDrawPos = SCREEN_YSIZE;

    DrawObjectList(0);
    if (activeTileLayers[0] < LAYER_COUNT) {
        switch (stageLayouts[activeTileLayers[0]].type) {
            case LAYER_HSCROLL: DrawHLineScrollLayer(0); break;
            case LAYER_VSCROLL: DrawVLineScrollLayer(0); break;
            case LAYER_3DFLOOR: Draw3DFloorLayer(0); break;
            case LAYER_3DSKY: Draw3DSkyLayer(0); break;
            default: break;
        }
    }

    DrawObjectList(1);
    if (activeTileLayers[1] < LAYER_COUNT) {
        switch (stageLayouts[activeTileLayers[1]].type) {
            case LAYER_HSCROLL: DrawHLineScrollLayer(1); break;
            case LAYER_VSCROLL: DrawVLineScrollLayer(1); break;
            case LAYER_3DFLOOR: Draw3DFloorLayer(1); break;
            case LAYER_3DSKY: Draw3DSkyLayer(1); break;
            default: break;
        }
    }

    DrawObjectList(2);
    if (activeTileLayers[2] < LAYER_COUNT) {
        switch (stageLayouts[activeTileLayers[2]].type) {
            case LAYER_HSCROLL: DrawHLineScrollLayer(2); break;
            case LAYER_VSCROLL: DrawVLineScrollLayer(2); break;
            case LAYER_3DFLOOR: Draw3DFloorLayer(2); break;
            case LAYER_3DSKY: Draw3DSkyLayer(2); break;
            default: break;
        }
    }

    DrawObjectList(3);
    DrawObjectList(4);
    if (activeTileLayers[3] < LAYER_COUNT) {
        switch (stageLayouts[activeTileLayers[3]].type) {
            case LAYER_HSCROLL: DrawHLineScrollLayer(3); break;
            case LAYER_VSCROLL: DrawVLineScrollLayer(3); break;
            case LAYER_3DFLOOR: Draw3DFloorLayer(3); break;
            case LAYER_3DSKY: Draw3DSkyLayer(3); break;
            default: break;
        }
    }

    DrawObjectList(5);
    DrawObjectList(6);

    if (fadeMode > 0) {
        DrawRectangle(0, 0, SCREEN_XSIZE, SCREEN_YSIZE, fadeR, fadeG, fadeB, fadeA);
    }
}

int tileXPos[PARALLAX_COUNT];
int tileYPos[PARALLAX_COUNT];

void DrawHLineScrollLayer(int layerID)
{
    TileLayer *layer = &stageLayouts[activeTileLayers[layerID]];
    int screenwidth16       = (SCREEN_XSIZE >> 4) - 1;
    int layerwidth          = layer->width;
    int layerheight         = layer->height;
    bool aboveMidPoint      = layerID >= tLayerMidPoint;

    byte *lineScroll;
    int *deformationData;
    int *deformationDataW;

    int yscrollOffset = 0;
    if (activeTileLayers[layerID]) { // BG Layer
        int yScroll    = yScrollOffset * layer->parallaxFactor >> 8;
        int fullheight = layerheight << 7;
        layer->scrollPos += layer->scrollSpeed;
        if (layer->scrollPos > fullheight << 16)
            layer->scrollPos -= fullheight << 16;
        yscrollOffset    = (yScroll + (layer->scrollPos >> 16)) % fullheight;
        layerheight      = fullheight >> 7;
        lineScroll       = layer->lineScroll;
        deformationData  = &bgDeformationData2[(byte)(yscrollOffset + layer->deformationOffset)];
        deformationDataW = &bgDeformationData3[(byte)(yscrollOffset + waterDrawPos + layer->deformationOffsetW)];
    }
    else { // FG Layer
        lastXSize = layer->width;
        yscrollOffset    = yScrollOffset;
        lineScroll       = layer->lineScroll;
        for (int i = 0; i < PARALLAX_COUNT; ++i) tileXPos[i] = xScrollOffset;
        deformationData  = &bgDeformationData0[(byte)(yscrollOffset + layer->deformationOffset)];
        deformationDataW = &bgDeformationData1[(byte)(yscrollOffset + waterDrawPos + layer->deformationOffsetW)];
    }

    if (layer->type == LAYER_HSCROLL) {
        if (lastXSize != layerwidth) {
            int fullLayerwidth = layerwidth << 7;
            for (int i = 0; i < hParallax.entryCount; ++i) {
                tileXPos[i]   = xScrollOffset * hParallax.parallaxFactor[i] >> 8;
                int scrollPos = hParallax.scrollPos[i];
                while (scrollPos > fullLayerwidth << 16) scrollPos -= fullLayerwidth << 16;
                while (scrollPos < 0) scrollPos += fullLayerwidth << 16;
                tileXPos[i] += scrollPos >> 16;
                tileXPos[i] %= fullLayerwidth;
            }
        }
        lastXSize = layerwidth;
    }

    ushort *frameBufferPtr = Engine.frameBuffer;
    byte *lineBuffer       = gfxLineBuffer;
    int tileYPos           = yscrollOffset % (layerheight << 7);
    if (tileYPos < 0)
        tileYPos += layerheight << 7;
    byte *scrollIndex = &lineScroll[tileYPos];
    int tileY16       = tileYPos & 0xF;
    int chunkY        = tileYPos >> 7;
    int tileY         = (tileYPos & 0x7F) >> 4;

    // Draw Above Water (if applicable)
    int drawableLines[2] = { waterDrawPos, SCREEN_YSIZE - waterDrawPos };
    for (int i = 0; i < 2; ++i) {
        while (drawableLines[i]--) {
            activePalette = fullPalette[*lineBuffer++];
            int chunkX             = tileXPos[*scrollIndex];
            if (i == 0) {
                if (hParallax.deform[*scrollIndex])
                    chunkX += *deformationData;
                ++deformationData;
            }
            else {
                if (hParallax.deform[*scrollIndex])
                    chunkX += *deformationDataW;
                ++deformationDataW;
            }
            ++scrollIndex;
            int fullLayerwidth = layerwidth << 7;
            if (chunkX < 0)
                chunkX += fullLayerwidth;
            if (chunkX >= fullLayerwidth)
                chunkX -= fullLayerwidth;
            int chunkXPos         = chunkX >> 7;
            int tilePxXPos        = chunkX & 0xF;
            int tileXPxRemain     = TILE_SIZE - tilePxXPos;
            int chunk             = (layer->tiles[(chunkX >> 7) + (chunkY << 8)] << 6) + ((chunkX & 0x7F) >> 4) + 8 * tileY;
            int tileOffsetY       = TILE_SIZE * tileY16;
            int tileOffsetYFlipX  = TILE_SIZE * tileY16 + 0xF;
            int tileOffsetYFlipY  = TILE_SIZE * (0xF - tileY16);
            int tileOffsetYFlipXY = TILE_SIZE * (0xF - tileY16) + 0xF;
            int lineRemain        = SCREEN_XSIZE;

            byte *gfxDataPtr  = NULL;
            int tilePxLineCnt = 0;

            //Draw the first tile to the left
            if (tiles128x128.visualPlane[chunk] == (byte)aboveMidPoint) {
                tilePxLineCnt = TILE_SIZE - tilePxXPos;
                lineRemain -= tilePxLineCnt;
                switch (tiles128x128.direction[chunk]) {
                    case FLIP_NONE:
                        gfxDataPtr    = &tilesetGFXData[tileOffsetY + tiles128x128.gfxDataPos[chunk] + tilePxXPos];
                        while (tilePxLineCnt--) {
                            if (*gfxDataPtr > 0)
                                *frameBufferPtr = activePalette[*gfxDataPtr];
                            ++frameBufferPtr;
                            ++gfxDataPtr;
                        }
                        break;
                    case FLIP_X:
                        gfxDataPtr    = &tilesetGFXData[tileOffsetYFlipX + tiles128x128.gfxDataPos[chunk] - tilePxXPos];
                        while (tilePxLineCnt--) {
                            if (*gfxDataPtr > 0)
                                *frameBufferPtr = activePalette[*gfxDataPtr];
                            ++frameBufferPtr;
                            --gfxDataPtr;
                        }
                        break;
                    case FLIP_Y:
                        gfxDataPtr    = &tilesetGFXData[tileOffsetYFlipY + tiles128x128.gfxDataPos[chunk] + tilePxXPos];
                        while (tilePxLineCnt--) {
                            if (*gfxDataPtr > 0)
                                *frameBufferPtr = activePalette[*gfxDataPtr];
                            ++frameBufferPtr;
                            ++gfxDataPtr;
                        }
                        break;
                    case FLIP_XY:
                        gfxDataPtr    = &tilesetGFXData[tileOffsetYFlipXY + tiles128x128.gfxDataPos[chunk] - tilePxXPos];
                        while (tilePxLineCnt--) {
                            if (*gfxDataPtr > 0)
                                *frameBufferPtr = activePalette[*gfxDataPtr];
                            ++frameBufferPtr;
                            --gfxDataPtr;
                        }
                        break;
                    default: break;
                }
            }
            else {
                frameBufferPtr += tileXPxRemain;
                lineRemain -= tileXPxRemain;
            }

            //Draw the bulk of the tiles
            int chunkTileX   = ((chunkX & 0x7F) >> 4) + 1;
            int tilesPerLine = screenwidth16;
            while (tilesPerLine--) {
                if (chunkTileX <= 7) {
                    ++chunk;
                }
                else {
                    if (++chunkXPos == layerwidth)
                        chunkXPos = 0;
                    chunkTileX = 0;
                    chunk      = (layer->tiles[chunkXPos + (chunkY << 8)] << 6) + 8 * tileY;
                }
                lineRemain -= TILE_SIZE;

                // Loop Unrolling (faster but messier code)
                if (tiles128x128.visualPlane[chunk] == (byte)aboveMidPoint) {
                    switch (tiles128x128.direction[chunk]) {
                        case FLIP_NONE:
                            gfxDataPtr = &tilesetGFXData[tiles128x128.gfxDataPos[chunk] + tileOffsetY];
                            if (*gfxDataPtr > 0)
                                *frameBufferPtr = activePalette[*gfxDataPtr];
                            ++frameBufferPtr;
                            ++gfxDataPtr;
                            if (*gfxDataPtr > 0)
                                *frameBufferPtr = activePalette[*gfxDataPtr];
                            ++frameBufferPtr;
                            ++gfxDataPtr;
                            if (*gfxDataPtr > 0)
                                *frameBufferPtr = activePalette[*gfxDataPtr];
                            ++frameBufferPtr;
                            ++gfxDataPtr;
                            if (*gfxDataPtr > 0)
                                *frameBufferPtr = activePalette[*gfxDataPtr];
                            ++frameBufferPtr;
                            ++gfxDataPtr;
                            if (*gfxDataPtr > 0)
                                *frameBufferPtr = activePalette[*gfxDataPtr];
                            ++frameBufferPtr;
                            ++gfxDataPtr;
                            if (*gfxDataPtr > 0)
                                *frameBufferPtr = activePalette[*gfxDataPtr];
                            ++frameBufferPtr;
                            ++gfxDataPtr;
                            if (*gfxDataPtr > 0)
                                *frameBufferPtr = activePalette[*gfxDataPtr];
                            ++frameBufferPtr;
                            ++gfxDataPtr;
                            if (*gfxDataPtr > 0)
                                *frameBufferPtr = activePalette[*gfxDataPtr];
                            ++frameBufferPtr;
                            ++gfxDataPtr;
                            if (*gfxDataPtr > 0)
                                *frameBufferPtr = activePalette[*gfxDataPtr];
                            ++frameBufferPtr;
                            ++gfxDataPtr;
                            if (*gfxDataPtr > 0)
                                *frameBufferPtr = activePalette[*gfxDataPtr];
                            ++frameBufferPtr;
                            ++gfxDataPtr;
                            if (*gfxDataPtr > 0)
                                *frameBufferPtr = activePalette[*gfxDataPtr];
                            ++frameBufferPtr;
                            ++gfxDataPtr;
                            if (*gfxDataPtr > 0)
                                *frameBufferPtr = activePalette[*gfxDataPtr];
                            ++frameBufferPtr;
                            ++gfxDataPtr;
                            if (*gfxDataPtr > 0)
                                *frameBufferPtr = activePalette[*gfxDataPtr];
                            ++frameBufferPtr;
                            ++gfxDataPtr;
                            if (*gfxDataPtr > 0)
                                *frameBufferPtr = activePalette[*gfxDataPtr];
                            ++frameBufferPtr;
                            ++gfxDataPtr;
                            if (*gfxDataPtr > 0)
                                *frameBufferPtr = activePalette[*gfxDataPtr];
                            ++frameBufferPtr;
                            ++gfxDataPtr;
                            if (*gfxDataPtr > 0)
                                *frameBufferPtr = activePalette[*gfxDataPtr];
                            ++frameBufferPtr;
                            ++gfxDataPtr;
                            break;
                        case FLIP_X:
                            gfxDataPtr = &tilesetGFXData[tiles128x128.gfxDataPos[chunk] + tileOffsetYFlipX];
                            if (*gfxDataPtr > 0)
                                *frameBufferPtr = activePalette[*gfxDataPtr];
                            ++frameBufferPtr;
                            --gfxDataPtr;
                            if (*gfxDataPtr > 0)
                                *frameBufferPtr = activePalette[*gfxDataPtr];
                            ++frameBufferPtr;
                            --gfxDataPtr;
                            if (*gfxDataPtr > 0)
                                *frameBufferPtr = activePalette[*gfxDataPtr];
                            ++frameBufferPtr;
                            --gfxDataPtr;
                            if (*gfxDataPtr > 0)
                                *frameBufferPtr = activePalette[*gfxDataPtr];
                            ++frameBufferPtr;
                            --gfxDataPtr;
                            if (*gfxDataPtr > 0)
                                *frameBufferPtr = activePalette[*gfxDataPtr];
                            ++frameBufferPtr;
                            --gfxDataPtr;
                            if (*gfxDataPtr > 0)
                                *frameBufferPtr = activePalette[*gfxDataPtr];
                            ++frameBufferPtr;
                            --gfxDataPtr;
                            if (*gfxDataPtr > 0)
                                *frameBufferPtr = activePalette[*gfxDataPtr];
                            ++frameBufferPtr;
                            --gfxDataPtr;
                            if (*gfxDataPtr > 0)
                                *frameBufferPtr = activePalette[*gfxDataPtr];
                            ++frameBufferPtr;
                            --gfxDataPtr;
                            if (*gfxDataPtr > 0)
                                *frameBufferPtr = activePalette[*gfxDataPtr];
                            ++frameBufferPtr;
                            --gfxDataPtr;
                            if (*gfxDataPtr > 0)
                                *frameBufferPtr = activePalette[*gfxDataPtr];
                            ++frameBufferPtr;
                            --gfxDataPtr;
                            if (*gfxDataPtr > 0)
                                *frameBufferPtr = activePalette[*gfxDataPtr];
                            ++frameBufferPtr;
                            --gfxDataPtr;
                            if (*gfxDataPtr > 0)
                                *frameBufferPtr = activePalette[*gfxDataPtr];
                            ++frameBufferPtr;
                            --gfxDataPtr;
                            if (*gfxDataPtr > 0)
                                *frameBufferPtr = activePalette[*gfxDataPtr];
                            ++frameBufferPtr;
                            --gfxDataPtr;
                            if (*gfxDataPtr > 0)
                                *frameBufferPtr = activePalette[*gfxDataPtr];
                            ++frameBufferPtr;
                            --gfxDataPtr;
                            if (*gfxDataPtr > 0)
                                *frameBufferPtr = activePalette[*gfxDataPtr];
                            ++frameBufferPtr;
                            --gfxDataPtr;
                            if (*gfxDataPtr > 0)
                                *frameBufferPtr = activePalette[*gfxDataPtr];
                            ++frameBufferPtr;
                            --gfxDataPtr;
                            break;
                        case FLIP_Y:
                            gfxDataPtr = &tilesetGFXData[tiles128x128.gfxDataPos[chunk] + tileOffsetYFlipY];
                            if (*gfxDataPtr > 0)
                                *frameBufferPtr = activePalette[*gfxDataPtr];
                            ++frameBufferPtr;
                            ++gfxDataPtr;
                            if (*gfxDataPtr > 0)
                                *frameBufferPtr = activePalette[*gfxDataPtr];
                            ++frameBufferPtr;
                            ++gfxDataPtr;
                            if (*gfxDataPtr > 0)
                                *frameBufferPtr = activePalette[*gfxDataPtr];
                            ++frameBufferPtr;
                            ++gfxDataPtr;
                            if (*gfxDataPtr > 0)
                                *frameBufferPtr = activePalette[*gfxDataPtr];
                            ++frameBufferPtr;
                            ++gfxDataPtr;
                            if (*gfxDataPtr > 0)
                                *frameBufferPtr = activePalette[*gfxDataPtr];
                            ++frameBufferPtr;
                            ++gfxDataPtr;
                            if (*gfxDataPtr > 0)
                                *frameBufferPtr = activePalette[*gfxDataPtr];
                            ++frameBufferPtr;
                            ++gfxDataPtr;
                            if (*gfxDataPtr > 0)
                                *frameBufferPtr = activePalette[*gfxDataPtr];
                            ++frameBufferPtr;
                            ++gfxDataPtr;
                            if (*gfxDataPtr > 0)
                                *frameBufferPtr = activePalette[*gfxDataPtr];
                            ++frameBufferPtr;
                            ++gfxDataPtr;
                            if (*gfxDataPtr > 0)
                                *frameBufferPtr = activePalette[*gfxDataPtr];
                            ++frameBufferPtr;
                            ++gfxDataPtr;
                            if (*gfxDataPtr > 0)
                                *frameBufferPtr = activePalette[*gfxDataPtr];
                            ++frameBufferPtr;
                            ++gfxDataPtr;
                            if (*gfxDataPtr > 0)
                                *frameBufferPtr = activePalette[*gfxDataPtr];
                            ++frameBufferPtr;
                            ++gfxDataPtr;
                            if (*gfxDataPtr > 0)
                                *frameBufferPtr = activePalette[*gfxDataPtr];
                            ++frameBufferPtr;
                            ++gfxDataPtr;
                            if (*gfxDataPtr > 0)
                                *frameBufferPtr = activePalette[*gfxDataPtr];
                            ++frameBufferPtr;
                            ++gfxDataPtr;
                            if (*gfxDataPtr > 0)
                                *frameBufferPtr = activePalette[*gfxDataPtr];
                            ++frameBufferPtr;
                            ++gfxDataPtr;
                            if (*gfxDataPtr > 0)
                                *frameBufferPtr = activePalette[*gfxDataPtr];
                            ++frameBufferPtr;
                            ++gfxDataPtr;
                            if (*gfxDataPtr > 0)
                                *frameBufferPtr = activePalette[*gfxDataPtr];
                            ++frameBufferPtr;
                            ++gfxDataPtr;
                            break;
                        case FLIP_XY:
                            gfxDataPtr = &tilesetGFXData[tiles128x128.gfxDataPos[chunk] + tileOffsetYFlipXY];
                            if (*gfxDataPtr > 0)
                                *frameBufferPtr = activePalette[*gfxDataPtr];
                            ++frameBufferPtr;
                            --gfxDataPtr;
                            if (*gfxDataPtr > 0)
                                *frameBufferPtr = activePalette[*gfxDataPtr];
                            ++frameBufferPtr;
                            --gfxDataPtr;
                            if (*gfxDataPtr > 0)
                                *frameBufferPtr = activePalette[*gfxDataPtr];
                            ++frameBufferPtr;
                            --gfxDataPtr;
                            if (*gfxDataPtr > 0)
                                *frameBufferPtr = activePalette[*gfxDataPtr];
                            ++frameBufferPtr;
                            --gfxDataPtr;
                            if (*gfxDataPtr > 0)
                                *frameBufferPtr = activePalette[*gfxDataPtr];
                            ++frameBufferPtr;
                            --gfxDataPtr;
                            if (*gfxDataPtr > 0)
                                *frameBufferPtr = activePalette[*gfxDataPtr];
                            ++frameBufferPtr;
                            --gfxDataPtr;
                            if (*gfxDataPtr > 0)
                                *frameBufferPtr = activePalette[*gfxDataPtr];
                            ++frameBufferPtr;
                            --gfxDataPtr;
                            if (*gfxDataPtr > 0)
                                *frameBufferPtr = activePalette[*gfxDataPtr];
                            ++frameBufferPtr;
                            --gfxDataPtr;
                            if (*gfxDataPtr > 0)
                                *frameBufferPtr = activePalette[*gfxDataPtr];
                            ++frameBufferPtr;
                            --gfxDataPtr;
                            if (*gfxDataPtr > 0)
                                *frameBufferPtr = activePalette[*gfxDataPtr];
                            ++frameBufferPtr;
                            --gfxDataPtr;
                            if (*gfxDataPtr > 0)
                                *frameBufferPtr = activePalette[*gfxDataPtr];
                            ++frameBufferPtr;
                            --gfxDataPtr;
                            if (*gfxDataPtr > 0)
                                *frameBufferPtr = activePalette[*gfxDataPtr];
                            ++frameBufferPtr;
                            --gfxDataPtr;
                            if (*gfxDataPtr > 0)
                                *frameBufferPtr = activePalette[*gfxDataPtr];
                            ++frameBufferPtr;
                            --gfxDataPtr;
                            if (*gfxDataPtr > 0)
                                *frameBufferPtr = activePalette[*gfxDataPtr];
                            ++frameBufferPtr;
                            --gfxDataPtr;
                            if (*gfxDataPtr > 0)
                                *frameBufferPtr = activePalette[*gfxDataPtr];
                            ++frameBufferPtr;
                            --gfxDataPtr;
                            if (*gfxDataPtr > 0)
                                *frameBufferPtr = activePalette[*gfxDataPtr];
                            ++frameBufferPtr;
                            --gfxDataPtr;
                            break;
                    }
                }
                else {
                    frameBufferPtr += 0x10;
                }
                ++chunkTileX;
            }

            //Draw any remaining tiles
            while (lineRemain > 0) {
                if (chunkTileX++ <= 7) {
                    ++chunk;
                }
                else {
                    chunkTileX = 0;
                    if (++chunkXPos == layerwidth)
                        chunkXPos = 0;
                    chunk = (layer->tiles[chunkXPos + (chunkY << 8)] << 6) + 8 * tileY;
                }

                tilePxLineCnt = lineRemain >= TILE_SIZE ? TILE_SIZE : lineRemain;
                lineRemain -= tilePxLineCnt;
                if (tiles128x128.visualPlane[chunk] == (byte)aboveMidPoint) {
                    switch (tiles128x128.direction[chunk]) {
                        case FLIP_NONE:
                            gfxDataPtr = &tilesetGFXData[tiles128x128.gfxDataPos[chunk] + tileOffsetY];
                            while (tilePxLineCnt--) {
                                if (*gfxDataPtr > 0)
                                    *frameBufferPtr = activePalette[*gfxDataPtr];
                                ++frameBufferPtr;
                                ++gfxDataPtr;
                            }
                            break;
                        case FLIP_X:
                            gfxDataPtr = &tilesetGFXData[tiles128x128.gfxDataPos[chunk] + tileOffsetYFlipX];
                            while (tilePxLineCnt--) {
                                if (*gfxDataPtr > 0)
                                    *frameBufferPtr = activePalette[*gfxDataPtr];
                                ++frameBufferPtr;
                                --gfxDataPtr;
                            }
                            break;
                        case FLIP_Y:
                            gfxDataPtr = &tilesetGFXData[tiles128x128.gfxDataPos[chunk] + tileOffsetYFlipY];
                            while (tilePxLineCnt--) {
                                if (*gfxDataPtr > 0)
                                    *frameBufferPtr = activePalette[*gfxDataPtr];
                                ++frameBufferPtr;
                                ++gfxDataPtr;
                            }
                            break;
                        case FLIP_XY:
                            gfxDataPtr = &tilesetGFXData[tiles128x128.gfxDataPos[chunk] + tileOffsetYFlipXY];
                            while (tilePxLineCnt--) {
                                if (*gfxDataPtr > 0)
                                    *frameBufferPtr = activePalette[*gfxDataPtr];
                                ++frameBufferPtr;
                                --gfxDataPtr;
                            }
                            break;
                        default: break;
                    }
                }
                else {
                    frameBufferPtr += tilePxLineCnt;
                }
            }

            if (++tileY16 > TILE_SIZE - 1) {
                tileY16 = 0;
                ++tileY;
            }
            if (tileY > 7) {
                if (++chunkY == layerheight) {
                    chunkY = 0;
                    scrollIndex -= 0x80 * layerheight;
                }
                tileY = 0;
            }
        }
    }
}
void DrawVLineScrollLayer(int layerID)
{
    TileLayer *layer = &stageLayouts[activeTileLayers[layerID]];
    int layerwidth          = layer->width;
    int layerheight         = layer->height;
    bool aboveMidPoint      = layerID >= tLayerMidPoint;

    byte *lineScroll;
    int *deformationData;

    int xscrollOffset = 0;
    if (activeTileLayers[layerID]) { // BG Layer
        int xScroll        = xScrollOffset * layer->parallaxFactor >> 8;
        int fullLayerwidth = layerwidth << 7;
        layer->scrollPos += layer->scrollSpeed;
        if (layer->scrollPos > fullLayerwidth << 16)
            layer->scrollPos -= fullLayerwidth << 16;
        xscrollOffset   = (xScroll + (layer->scrollPos >> 16)) % fullLayerwidth;
        layerwidth      = fullLayerwidth >> 7;
        lineScroll      = layer->lineScroll;
        deformationData = &bgDeformationData2[(byte)(xscrollOffset + layer->deformationOffset)];
    }
    else { // FG Layer
        lastYSize           = layer->height;
        xscrollOffset              = xScrollOffset;
        lineScroll                 = layer->lineScroll;
        tileYPos[0]                = yScrollOffset;
        vParallax.deform[0] = true;
        deformationData            = &bgDeformationData0[(byte)(xScrollOffset + layer->deformationOffset)];
    }

    if (layer->type == LAYER_VSCROLL) {
        if (lastYSize != layerheight) {
            int fullLayerheight = layerheight << 7;
            for (int i = 0; i < vParallax.entryCount; ++i) {
                tileYPos[i]   = xScrollOffset * vParallax.parallaxFactor[i] >> 8;
                int scrollPos = vParallax.scrollPos[i];
                while (scrollPos > fullLayerheight << 16) scrollPos -= fullLayerheight << 16;
                while (scrollPos < 0) scrollPos += fullLayerheight << 16;
                tileYPos[i] += scrollPos >> 16;
                tileYPos[i] %= fullLayerheight;
            }
            layerheight = fullLayerheight >> 7;
        }
        lastYSize = layerwidth;
    }

    ushort *frameBufferPtr = Engine.frameBuffer;
    activePalette = fullPalette[gfxLineBuffer[0]];
    int tileXPos           = xscrollOffset % (layerheight << 7);
    if (tileXPos < 0)
        tileXPos += layerheight << 7;
    byte *scrollIndex = &lineScroll[tileXPos];
    int tileX16       = tileXPos & 0xF;
    int tileX         = (tileXPos & 0x7F) >> 4;

    // Draw Above Water (if applicable)
    int drawableLines = waterDrawPos;
    while (drawableLines--) {
        int chunkY = tileYPos[*scrollIndex];
        if (vParallax.deform[*scrollIndex])
            chunkY += *deformationData;
        ++deformationData;
        ++scrollIndex;
        int fullLayerHeight = layerheight << 7;
        if (chunkY < 0)
            chunkY += fullLayerHeight;
        if (chunkY >= fullLayerHeight)
            chunkY -= fullLayerHeight;
        int chunkYPos         = chunkY >> 7;
        int tileY             = chunkY & 0xF;
        int tileYPxRemain     = 0x10 - tileY;
        int chunk             = (layer->tiles[chunkY + (chunkY >> 7 << 8)] << 6) + tileX + 8 * ((chunkY & 0x7F) >> 4);
        int tileOffsetXFlipX  = 0xF - tileX16;
        int tileOffsetXFlipY  = tileX16 + SCREEN_YSIZE;
        int tileOffsetXFlipXY = 0xFF - tileX16;
        int lineRemain        = SCREEN_YSIZE;

        byte *gfxDataPtr  = NULL;
        int tilePxLineCnt = 0;

        // Draw the first tile to the left
        if (tiles128x128.visualPlane[chunk] == (byte)aboveMidPoint) {
            tilePxLineCnt = 0x10 - tileY;
            lineRemain -= tilePxLineCnt;
            switch (tiles128x128.direction[chunk]) {
                case FLIP_NONE:
                    gfxDataPtr    = &tilesetGFXData[0x10 * tileY + tileX16 + tiles128x128.gfxDataPos[chunk]];
                    while (tilePxLineCnt--) {
                        if (*gfxDataPtr > 0)
                            *frameBufferPtr = activePalette[*gfxDataPtr];
                        frameBufferPtr += SCREEN_XSIZE;
                        gfxDataPtr += 0x10;
                    }
                    break;
                case FLIP_X:
                    gfxDataPtr    = &tilesetGFXData[0x10 * tileY + tileOffsetXFlipX + tiles128x128.gfxDataPos[chunk]];
                    while (tilePxLineCnt--) {
                        if (*gfxDataPtr > 0)
                            *frameBufferPtr = activePalette[*gfxDataPtr];
                        frameBufferPtr += SCREEN_XSIZE;
                        gfxDataPtr += 0x10;
                    }
                    break;
                case FLIP_Y:
                    gfxDataPtr    = &tilesetGFXData[tileOffsetXFlipY + tiles128x128.gfxDataPos[chunk] - 0x10 * tileY];
                    while (tilePxLineCnt--) {
                        if (*gfxDataPtr > 0)
                            *frameBufferPtr = activePalette[*gfxDataPtr];
                        frameBufferPtr += SCREEN_XSIZE;
                        gfxDataPtr -= 0x10;
                    }
                    break;
                case FLIP_XY:
                    gfxDataPtr    = &tilesetGFXData[tileOffsetXFlipXY + tiles128x128.gfxDataPos[chunk] - 16 * tileY];
                    while (tilePxLineCnt--) {
                        if (*gfxDataPtr > 0)
                            *frameBufferPtr = activePalette[*gfxDataPtr];
                        frameBufferPtr += SCREEN_XSIZE;
                        gfxDataPtr -= 0x10;
                    }
                    break;
                default: break;
            }
        }
        else {
            frameBufferPtr += SCREEN_XSIZE * tileYPxRemain;
            lineRemain -= tileYPxRemain;
        }

        //Draw the bulk of the tiles
        int chunkTileY   = ((chunkY & 0x7F) >> 4) + 1;
        int tilesPerLine = 14;
        while (tilesPerLine--) {
            if (chunkTileY <= 7) {
                chunk += 8;
            }
            else {
                if (++chunkYPos == layerheight)
                    chunkYPos = 0;
                chunkTileY = 0;
                chunk      = (layer->tiles[chunkY + (chunkYPos << 8)] << 6) + tileX;
            }
            lineRemain -= TILE_SIZE;

            // Loop Unrolling (faster but messier code)
            if (tiles128x128.visualPlane[chunk] == (byte)aboveMidPoint) {
                switch (tiles128x128.direction[chunk]) {
                    case FLIP_NONE:
                        gfxDataPtr = &tilesetGFXData[tiles128x128.gfxDataPos[chunk] + tileX16];
                        if (*gfxDataPtr > 0)
                            *frameBufferPtr = activePalette[*gfxDataPtr];
                        frameBufferPtr += SCREEN_XSIZE;
                        gfxDataPtr += 0x10;
                        if (*gfxDataPtr > 0)
                            *frameBufferPtr = activePalette[*gfxDataPtr];
                        frameBufferPtr += SCREEN_XSIZE;
                        gfxDataPtr += 0x10;
                        if (*gfxDataPtr > 0)
                            *frameBufferPtr = activePalette[*gfxDataPtr];
                        frameBufferPtr += SCREEN_XSIZE;
                        gfxDataPtr += 0x10;
                        if (*gfxDataPtr > 0)
                            *frameBufferPtr = activePalette[*gfxDataPtr];
                        frameBufferPtr += SCREEN_XSIZE;
                        gfxDataPtr += 0x10;
                        if (*gfxDataPtr > 0)
                            *frameBufferPtr = activePalette[*gfxDataPtr];
                        frameBufferPtr += SCREEN_XSIZE;
                        gfxDataPtr += 0x10;
                        if (*gfxDataPtr > 0)
                            *frameBufferPtr = activePalette[*gfxDataPtr];
                        frameBufferPtr += SCREEN_XSIZE;
                        gfxDataPtr += 0x10;
                        if (*gfxDataPtr > 0)
                            *frameBufferPtr = activePalette[*gfxDataPtr];
                        frameBufferPtr += SCREEN_XSIZE;
                        gfxDataPtr += 0x10;
                        if (*gfxDataPtr > 0)
                            *frameBufferPtr = activePalette[*gfxDataPtr];
                        frameBufferPtr += SCREEN_XSIZE;
                        gfxDataPtr += 0x10;
                        if (*gfxDataPtr > 0)
                            *frameBufferPtr = activePalette[*gfxDataPtr];
                        frameBufferPtr += SCREEN_XSIZE;
                        gfxDataPtr += 0x10;
                        if (*gfxDataPtr > 0)
                            *frameBufferPtr = activePalette[*gfxDataPtr];
                        frameBufferPtr += SCREEN_XSIZE;
                        gfxDataPtr += 0x10;
                        if (*gfxDataPtr > 0)
                            *frameBufferPtr = activePalette[*gfxDataPtr];
                        frameBufferPtr += SCREEN_XSIZE;
                        gfxDataPtr += 0x10;
                        if (*gfxDataPtr > 0)
                            *frameBufferPtr = activePalette[*gfxDataPtr];
                        frameBufferPtr += SCREEN_XSIZE;
                        gfxDataPtr += 0x10;
                        if (*gfxDataPtr > 0)
                            *frameBufferPtr = activePalette[*gfxDataPtr];
                        frameBufferPtr += SCREEN_XSIZE;
                        gfxDataPtr += 0x10;
                        if (*gfxDataPtr > 0)
                            *frameBufferPtr = activePalette[*gfxDataPtr];
                        frameBufferPtr += SCREEN_XSIZE;
                        gfxDataPtr += 0x10;
                        if (*gfxDataPtr > 0)
                            *frameBufferPtr = activePalette[*gfxDataPtr];
                        frameBufferPtr += SCREEN_XSIZE;
                        gfxDataPtr += 0x10;
                        if (*gfxDataPtr > 0)
                            *frameBufferPtr = activePalette[*gfxDataPtr];
                        frameBufferPtr += SCREEN_XSIZE;
                        gfxDataPtr += 0x10;
                        break;
                    case FLIP_X:
                        gfxDataPtr = &tilesetGFXData[tiles128x128.gfxDataPos[chunk] + tileOffsetXFlipX];
                        if (*gfxDataPtr > 0)
                            *frameBufferPtr = activePalette[*gfxDataPtr];
                        frameBufferPtr += SCREEN_XSIZE;
                        gfxDataPtr += 0x10;
                        if (*gfxDataPtr > 0)
                            *frameBufferPtr = activePalette[*gfxDataPtr];
                        frameBufferPtr += SCREEN_XSIZE;
                        gfxDataPtr += 0x10;
                        if (*gfxDataPtr > 0)
                            *frameBufferPtr = activePalette[*gfxDataPtr];
                        frameBufferPtr += SCREEN_XSIZE;
                        gfxDataPtr += 0x10;
                        if (*gfxDataPtr > 0)
                            *frameBufferPtr = activePalette[*gfxDataPtr];
                        frameBufferPtr += SCREEN_XSIZE;
                        gfxDataPtr += 0x10;
                        if (*gfxDataPtr > 0)
                            *frameBufferPtr = activePalette[*gfxDataPtr];
                        frameBufferPtr += SCREEN_XSIZE;
                        gfxDataPtr += 0x10;
                        if (*gfxDataPtr > 0)
                            *frameBufferPtr = activePalette[*gfxDataPtr];
                        frameBufferPtr += SCREEN_XSIZE;
                        gfxDataPtr += 0x10;
                        if (*gfxDataPtr > 0)
                            *frameBufferPtr = activePalette[*gfxDataPtr];
                        frameBufferPtr += SCREEN_XSIZE;
                        gfxDataPtr += 0x10;
                        if (*gfxDataPtr > 0)
                            *frameBufferPtr = activePalette[*gfxDataPtr];
                        frameBufferPtr += SCREEN_XSIZE;
                        gfxDataPtr += 0x10;
                        if (*gfxDataPtr > 0)
                            *frameBufferPtr = activePalette[*gfxDataPtr];
                        frameBufferPtr += SCREEN_XSIZE;
                        gfxDataPtr += 0x10;
                        if (*gfxDataPtr > 0)
                            *frameBufferPtr = activePalette[*gfxDataPtr];
                        frameBufferPtr += SCREEN_XSIZE;
                        gfxDataPtr += 0x10;
                        if (*gfxDataPtr > 0)
                            *frameBufferPtr = activePalette[*gfxDataPtr];
                        frameBufferPtr += SCREEN_XSIZE;
                        gfxDataPtr += 0x10;
                        if (*gfxDataPtr > 0)
                            *frameBufferPtr = activePalette[*gfxDataPtr];
                        frameBufferPtr += SCREEN_XSIZE;
                        gfxDataPtr += 0x10;
                        if (*gfxDataPtr > 0)
                            *frameBufferPtr = activePalette[*gfxDataPtr];
                        frameBufferPtr += SCREEN_XSIZE;
                        gfxDataPtr += 0x10;
                        if (*gfxDataPtr > 0)
                            *frameBufferPtr = activePalette[*gfxDataPtr];
                        frameBufferPtr += SCREEN_XSIZE;
                        gfxDataPtr += 0x10;
                        if (*gfxDataPtr > 0)
                            *frameBufferPtr = activePalette[*gfxDataPtr];
                        frameBufferPtr += SCREEN_XSIZE;
                        gfxDataPtr += 0x10;
                        if (*gfxDataPtr > 0)
                            *frameBufferPtr = activePalette[*gfxDataPtr];
                        frameBufferPtr += SCREEN_XSIZE;
                        gfxDataPtr += 0x10;
                        break;
                    case FLIP_Y:
                        gfxDataPtr = &tilesetGFXData[tiles128x128.gfxDataPos[chunk] + tileOffsetXFlipY];
                        if (*gfxDataPtr > 0)
                            *frameBufferPtr = activePalette[*gfxDataPtr];
                        frameBufferPtr += SCREEN_XSIZE;
                        gfxDataPtr -= 0x10;
                        if (*gfxDataPtr > 0)
                            *frameBufferPtr = activePalette[*gfxDataPtr];
                        frameBufferPtr += SCREEN_XSIZE;
                        gfxDataPtr -= 0x10;
                        if (*gfxDataPtr > 0)
                            *frameBufferPtr = activePalette[*gfxDataPtr];
                        frameBufferPtr += SCREEN_XSIZE;
                        gfxDataPtr -= 0x10;
                        if (*gfxDataPtr > 0)
                            *frameBufferPtr = activePalette[*gfxDataPtr];
                        frameBufferPtr += SCREEN_XSIZE;
                        gfxDataPtr -= 0x10;
                        if (*gfxDataPtr > 0)
                            *frameBufferPtr = activePalette[*gfxDataPtr];
                        frameBufferPtr += SCREEN_XSIZE;
                        gfxDataPtr -= 0x10;
                        if (*gfxDataPtr > 0)
                            *frameBufferPtr = activePalette[*gfxDataPtr];
                        frameBufferPtr += SCREEN_XSIZE;
                        gfxDataPtr -= 0x10;
                        if (*gfxDataPtr > 0)
                            *frameBufferPtr = activePalette[*gfxDataPtr];
                        frameBufferPtr += SCREEN_XSIZE;
                        gfxDataPtr -= 0x10;
                        if (*gfxDataPtr > 0)
                            *frameBufferPtr = activePalette[*gfxDataPtr];
                        frameBufferPtr += SCREEN_XSIZE;
                        gfxDataPtr -= 0x10;
                        if (*gfxDataPtr > 0)
                            *frameBufferPtr = activePalette[*gfxDataPtr];
                        frameBufferPtr += SCREEN_XSIZE;
                        gfxDataPtr -= 0x10;
                        if (*gfxDataPtr > 0)
                            *frameBufferPtr = activePalette[*gfxDataPtr];
                        frameBufferPtr += SCREEN_XSIZE;
                        gfxDataPtr -= 0x10;
                        if (*gfxDataPtr > 0)
                            *frameBufferPtr = activePalette[*gfxDataPtr];
                        frameBufferPtr += SCREEN_XSIZE;
                        gfxDataPtr -= 0x10;
                        if (*gfxDataPtr > 0)
                            *frameBufferPtr = activePalette[*gfxDataPtr];
                        frameBufferPtr += SCREEN_XSIZE;
                        gfxDataPtr -= 0x10;
                        if (*gfxDataPtr > 0)
                            *frameBufferPtr = activePalette[*gfxDataPtr];
                        frameBufferPtr += SCREEN_XSIZE;
                        gfxDataPtr -= 0x10;
                        if (*gfxDataPtr > 0)
                            *frameBufferPtr = activePalette[*gfxDataPtr];
                        frameBufferPtr += SCREEN_XSIZE;
                        gfxDataPtr -= 0x10;
                        if (*gfxDataPtr > 0)
                            *frameBufferPtr = activePalette[*gfxDataPtr];
                        frameBufferPtr += SCREEN_XSIZE;
                        gfxDataPtr -= 0x10;
                        if (*gfxDataPtr > 0)
                            *frameBufferPtr = activePalette[*gfxDataPtr];
                        frameBufferPtr += SCREEN_XSIZE;
                        gfxDataPtr -= 0x10;
                        break;
                    case FLIP_XY:
                        gfxDataPtr = &tilesetGFXData[tiles128x128.gfxDataPos[chunk] + tileOffsetXFlipXY];
                        if (*gfxDataPtr > 0)
                            *frameBufferPtr = activePalette[*gfxDataPtr];
                        frameBufferPtr += SCREEN_XSIZE;
                        gfxDataPtr -= 0x10;
                        if (*gfxDataPtr > 0)
                            *frameBufferPtr = activePalette[*gfxDataPtr];
                        frameBufferPtr += SCREEN_XSIZE;
                        gfxDataPtr -= 0x10;
                        if (*gfxDataPtr > 0)
                            *frameBufferPtr = activePalette[*gfxDataPtr];
                        frameBufferPtr += SCREEN_XSIZE;
                        gfxDataPtr -= 0x10;
                        if (*gfxDataPtr > 0)
                            *frameBufferPtr = activePalette[*gfxDataPtr];
                        frameBufferPtr += SCREEN_XSIZE;
                        gfxDataPtr -= 0x10;
                        if (*gfxDataPtr > 0)
                            *frameBufferPtr = activePalette[*gfxDataPtr];
                        frameBufferPtr += SCREEN_XSIZE;
                        gfxDataPtr -= 0x10;
                        if (*gfxDataPtr > 0)
                            *frameBufferPtr = activePalette[*gfxDataPtr];
                        frameBufferPtr += SCREEN_XSIZE;
                        gfxDataPtr -= 0x10;
                        if (*gfxDataPtr > 0)
                            *frameBufferPtr = activePalette[*gfxDataPtr];
                        frameBufferPtr += SCREEN_XSIZE;
                        gfxDataPtr -= 0x10;
                        if (*gfxDataPtr > 0)
                            *frameBufferPtr = activePalette[*gfxDataPtr];
                        frameBufferPtr += SCREEN_XSIZE;
                        gfxDataPtr -= 0x10;
                        if (*gfxDataPtr > 0)
                            *frameBufferPtr = activePalette[*gfxDataPtr];
                        frameBufferPtr += SCREEN_XSIZE;
                        gfxDataPtr -= 0x10;
                        if (*gfxDataPtr > 0)
                            *frameBufferPtr = activePalette[*gfxDataPtr];
                        frameBufferPtr += SCREEN_XSIZE;
                        gfxDataPtr -= 0x10;
                        if (*gfxDataPtr > 0)
                            *frameBufferPtr = activePalette[*gfxDataPtr];
                        frameBufferPtr += SCREEN_XSIZE;
                        gfxDataPtr -= 0x10;
                        if (*gfxDataPtr > 0)
                            *frameBufferPtr = activePalette[*gfxDataPtr];
                        frameBufferPtr += SCREEN_XSIZE;
                        gfxDataPtr -= 0x10;
                        if (*gfxDataPtr > 0)
                            *frameBufferPtr = activePalette[*gfxDataPtr];
                        frameBufferPtr += SCREEN_XSIZE;
                        gfxDataPtr -= 0x10;
                        if (*gfxDataPtr > 0)
                            *frameBufferPtr = activePalette[*gfxDataPtr];
                        frameBufferPtr += SCREEN_XSIZE;
                        gfxDataPtr -= 0x10;
                        if (*gfxDataPtr > 0)
                            *frameBufferPtr = activePalette[*gfxDataPtr];
                        frameBufferPtr += SCREEN_XSIZE;
                        gfxDataPtr -= 0x10;
                        if (*gfxDataPtr > 0)
                            *frameBufferPtr = activePalette[*gfxDataPtr];
                        frameBufferPtr += SCREEN_XSIZE;
                        gfxDataPtr -= 0x10;
                        break;
                }
            }
            else {
                frameBufferPtr += 0x10;
            }
            ++chunkTileY;
        }

        // Draw any remaining tiles
        while (lineRemain > 0) {
            if (chunkTileY++ <= 7) {
                chunk += 8;
            }
            else {
                chunkTileY = 0;
                if (++chunkYPos == layerheight)
                    chunkYPos = 0;
                chunkTileY = 0;
                chunk      = (layer->tiles[chunkY + (chunkYPos << 8)] << 6) + tileX;
            }

            tilePxLineCnt = lineRemain >= TILE_SIZE ? TILE_SIZE : lineRemain;
            lineRemain -= tilePxLineCnt;
            if (tiles128x128.visualPlane[chunk] == (byte)aboveMidPoint) {
                switch (tiles128x128.direction[chunk]) {
                    case FLIP_NONE:
                        gfxDataPtr    = &tilesetGFXData[0x10 * tileY + tileX16 + tiles128x128.gfxDataPos[chunk]];
                        while (tilePxLineCnt--) {
                            if (*gfxDataPtr > 0)
                                *frameBufferPtr = activePalette[*gfxDataPtr];
                            frameBufferPtr += SCREEN_XSIZE;
                            gfxDataPtr += 0x10;
                        }
                        break;
                    case FLIP_X:
                        gfxDataPtr    = &tilesetGFXData[0x10 * tileY + tileOffsetXFlipX + tiles128x128.gfxDataPos[chunk]];
                        while (tilePxLineCnt--) {
                            if (*gfxDataPtr > 0)
                                *frameBufferPtr = activePalette[*gfxDataPtr];
                            frameBufferPtr += SCREEN_XSIZE;
                            gfxDataPtr += 0x10;
                        }
                        break;
                    case FLIP_Y:
                        gfxDataPtr    = &tilesetGFXData[tileOffsetXFlipY + tiles128x128.gfxDataPos[chunk] - 0x10 * tileY];
                        while (tilePxLineCnt--) {
                            if (*gfxDataPtr > 0)
                                *frameBufferPtr = activePalette[*gfxDataPtr];
                            frameBufferPtr += SCREEN_XSIZE;
                            gfxDataPtr -= 0x10;
                        }
                        break;
                    case FLIP_XY:
                        gfxDataPtr    = &tilesetGFXData[tileOffsetXFlipXY + tiles128x128.gfxDataPos[chunk] - 16 * tileY];
                        while (tilePxLineCnt--) {
                            if (*gfxDataPtr > 0)
                                *frameBufferPtr = activePalette[*gfxDataPtr];
                            frameBufferPtr += SCREEN_XSIZE;
                            gfxDataPtr -= 0x10;
                        }
                        break;
                    default: break;
                }
            }
            else {
                frameBufferPtr += SCREEN_XSIZE * tileYPxRemain;
            }
        }

        if (++tileX16 > 0xF) {
            tileX16 = 0;
            ++tileX;
        }
        if (tileX > 7) {
            if (++chunkY == layerwidth) {
                chunkY = 0;
                scrollIndex -= 0x80 * layerwidth;
            }
            tileX = 0;
        }
    }
}
void Draw3DFloorLayer(int layerID)
{
    TileLayer *layer = &stageLayouts[activeTileLayers[layerID]];
    int layerWidth          = layer->width << 7;
    int layerHeight         = layer->height << 7;
    int layerYPos           = layer->YPos;
    int layerZPos           = layer->ZPos;
    int sinValue            = sinM[layer->angle];
    int cosValue            = cosM[layer->angle];
    byte *linePtr           = gfxLineBuffer;
    ushort *frameBufferPtr  = &Engine.frameBuffer[132 * SCREEN_XSIZE];
    int layerXPos           = layer->XPos >> 4;
    int ZBuffer             = layerZPos >> 4;
    for (int i = 4; i < 112; ++i) {
        if (!(i & 1))
            activePalette = fullPalette[*linePtr++];
        int XBuffer            = layerYPos / (i << 9) * -cosValue >> 8;
        int YBuffer            = sinValue * (layerYPos / (i << 9)) >> 8;
        int XPos               = layerXPos + (3 * sinValue * (layerYPos / (i << 9)) >> 2) - XBuffer * SCREEN_CENTERX;
        int YPos               = ZBuffer + (3 * cosValue * (layerYPos / (i << 9)) >> 2) - YBuffer * SCREEN_CENTERX;
        int lineBuffer         = 0;
        while (lineBuffer < SCREEN_XSIZE) {
            int tileX = XPos >> 12;
            int tileY = YPos >> 12;
            if (tileX > -1 && tileX < layerWidth && tileY > -1 && tileY < layerHeight) {
                int chunk       = tile3DFloorBuffer[(YPos >> 16 << 8) + (XPos >> 16)];
                byte *tilePixel = &tilesetGFXData[tiles128x128.gfxDataPos[chunk]];
                switch (tiles128x128.direction[chunk]) {
                    case FLIP_NONE: tilePixel += 16 * (tileY & 0xF) + (tileX & 0xF); break;
                    case FLIP_X: tilePixel += 16 * (tileY & 0xF) + 15 - (tileX & 0xF); break;
                    case FLIP_Y: tilePixel += (tileX & 0xF) + SCREEN_YSIZE - 16 * (tileY & 0xF); break;
                    case FLIP_XY: tilePixel += 15 - (tileX & 0xF) + SCREEN_YSIZE - 16 * (tileY & 0xF); break;
                    default: break;
                }
                if (*tilePixel > 0)
                    *frameBufferPtr = activePalette[*tilePixel];
            }
            ++frameBufferPtr;
            ++lineBuffer;
            XPos += XBuffer;
            YPos += YBuffer;
        }
    }
}
void Draw3DSkyLayer(int layerID)
{
    TileLayer *layer = &stageLayouts[activeTileLayers[layerID]];
    int layerWidth          = layer->width << 7;
    int layerHeight         = layer->height << 7;
    int layerYPos           = layer->YPos;
    int sinValue            = sinM[layer->angle & 0x1FF];
    int cosValue            = cosM[layer->angle & 0x1FF];
    ushort *frameBufferPtr  = &Engine.frameBuffer[132 * SCREEN_XSIZE];
    byte *linePtr           = &gfxLineBuffer[132];
    int layerXPos           = layer->XPos >> 4;
    int layerZPos           = layer->ZPos >> 4;
    for (int i = TILE_SIZE / 2; i < SCREEN_YSIZE - TILE_SIZE; ++i) {
        if (!(i & 1))
            activePalette  = fullPalette[*linePtr++];
        int xBuffer    = layerYPos / (i << 8) * -cosValue >> 9;
        int yBuffer    = sinValue * (layerYPos / (i << 8)) >> 9;
        int XPos       = layerXPos + (3 * sinValue * (layerYPos / (i << 8)) >> 2) - xBuffer * SCREEN_XSIZE;
        int YPos       = layerZPos + (3 * cosValue * (layerYPos / (i << 8)) >> 2) - yBuffer * SCREEN_XSIZE;
        int lineBuffer = 0;
        while (lineBuffer < SCREEN_XSIZE * 2) {
            int tileX = XPos >> 12;
            int tileY = YPos >> 12;
            if (tileX > -1 && tileX < layerWidth && tileY > -1 && tileY < layerHeight) {
                int chunk       = tile3DFloorBuffer[(YPos >> 16 << 8) + (XPos >> 16)];
                byte *tilePixel = &tilesetGFXData[tiles128x128.gfxDataPos[chunk]];
                switch (tiles128x128.direction[chunk]) {
                    case FLIP_NONE: tilePixel += TILE_SIZE * (tileY & 0xF) + (tileX & 0xF); break;
                    case FLIP_X: tilePixel += TILE_SIZE * (tileY & 0xF) + 0xF - (tileX & 0xF); break;
                    case FLIP_Y: tilePixel += (tileX & 0xF) + SCREEN_YSIZE - TILE_SIZE * (tileY & 0xF); break;
                    case FLIP_XY: tilePixel += 0xF - (tileX & 0xF) + SCREEN_YSIZE - TILE_SIZE * (tileY & 0xF); break;
                    default: break;
                }
                if (*tilePixel > 0)
                    *frameBufferPtr = activePalette[*tilePixel];
            }
            if (lineBuffer & 1)
                ++frameBufferPtr;
            lineBuffer++;
            XPos += xBuffer;
            YPos += yBuffer;
        }
        if (!(i & 1))
            frameBufferPtr -= SCREEN_XSIZE;
    }

    //TODO(?): this is run only when the code above is drawn to a "HQ" framebuffer
    //if (false) {
    //    frameBufferPtr = &Engine.frameBuffer[132 * SCREEN_XSIZE];
    //    int cnt    = 108 * SCREEN_XSIZE;
    //    while (cnt--) *frameBufferPtr++ = 0xF81Fu; //Magenta
    //}
}

void DrawRectangle(int XPos, int YPos, int width, int height, int R, int G, int B, int A)
{
    if (width + XPos > SCREEN_XSIZE)
        width = SCREEN_XSIZE - XPos;
    if (XPos < 0) {
        width += XPos;
        XPos = 0;
    }

    if (height + YPos > SCREEN_YSIZE)
        height = SCREEN_YSIZE - YPos;
    if (YPos < 0) {
        height += YPos;
        YPos = 0;
    }
    if (width <= 0 || height <= 0 || A <= 0)
        return;
    if (A > 0xFF)
        A = 0xFF;
    int pitch              = SCREEN_XSIZE - width;
    ushort *frameBufferPtr = &Engine.frameBuffer[XPos + SCREEN_XSIZE * YPos];
    ushort clr             = (B >> 3) | 32 * (G >> 2) | ((ushort)(R >> 3) << 11);
    if (A == 0xFF) {
        int h = height;
        while (h--) {
            int w = width;
            while (w--) {
                *frameBufferPtr = clr;
                ++frameBufferPtr;
            }
            frameBufferPtr += pitch;
        }
    }
    else {
        int h = height;
        while (h--) {
            int w = width;
            while (w--) {
                short *blendPtrB = &blendLookupTable[BLENDTABLE_XSIZE * (0xFF - A)];
                short *blendPtrA = &blendLookupTable[BLENDTABLE_XSIZE * A];
                *frameBufferPtr  = (blendPtrB[*frameBufferPtr & (BLENDTABLE_XSIZE-1)] + blendPtrA[((byte)(B >> 3) | (byte)(32 * (G >> 2))) & 0x1F])
                                  | ((blendPtrB[(*frameBufferPtr & 0x7E0) >> 6] + blendPtrA[(clr & 0x7E0) >> 6]) << 6)
                                  | ((blendPtrB[(*frameBufferPtr & 0xF800) >> 11] + blendPtrA[(clr & 0xF800) >> 11]) << 11);
                ++frameBufferPtr;
            }
            frameBufferPtr += pitch;
        }
    }
}

void DrawTintRectangle(int XPos, int YPos, int width, int height)
{
    if (width + XPos > SCREEN_XSIZE)
        width = SCREEN_XSIZE - XPos;
    if (XPos < 0) {
        width += XPos;
        XPos = 0;
    }

    if (height + YPos > SCREEN_YSIZE)
        height = SCREEN_YSIZE - YPos;
    if (YPos < 0) {
        height += YPos;
        YPos = 0;
    }
    if (width <= 0 || height <= 0)
        return;
    int yOffset = SCREEN_XSIZE - width;
    for (ushort *frameBufferPtr = &Engine.frameBuffer[XPos + SCREEN_XSIZE * YPos];; frameBufferPtr += yOffset) {
        height--;
        if (!height)
            break;
        int w = width;
        while (w--) {
            *frameBufferPtr = tintLookupTable[*frameBufferPtr];
            ++frameBufferPtr;
        }
    }
}
void DrawScaledTintMask(int direction, int XPos, int YPos, int pivotX, int pivotY, int scaleX, int scaleY, int width, int height, int sprX,
                                 int sprY, int sheetID)
{
    int roundedYPos = 0;
    int roundedXPos = 0;
    int truescaleX  = 4 * scaleX;
    int truescaleY  = 4 * scaleY;
    int widthM1     = width - 1;
    int trueXPos    = XPos - (truescaleX * pivotX >> 11);
    width           = truescaleX * width >> 11;
    int trueYPos    = YPos - (truescaleY * pivotY >> 11);
    height          = truescaleY * height >> 11;
    int finalscaleX = (signed int)(float)((float)(2048.0 / (float)truescaleX) * 2048.0);
    int finalscaleY = (signed int)(float)((float)(2048.0 / (float)truescaleY) * 2048.0);
    if (width + trueXPos > SCREEN_XSIZE) {
        width = SCREEN_XSIZE - trueXPos;
    }

    if (direction) {
        if (trueXPos < 0) {
            widthM1 -= trueXPos * -finalscaleX >> 11;
            roundedXPos = (ushort)trueXPos * -(short)finalscaleX & 0x7FF;
            width += trueXPos;
            trueXPos = 0;
        }
    }
    else if (trueXPos < 0) {
        sprX += trueXPos * -finalscaleX >> 11;
        roundedXPos = (ushort)trueXPos * -(short)finalscaleX & 0x7FF;
        width += trueXPos;
        trueXPos = 0;
    }

    if (height + trueYPos > SCREEN_YSIZE) {
        height = SCREEN_YSIZE - trueYPos;
    }
    if (trueYPos < 0) {
        sprY += trueYPos * -finalscaleY >> 11;
        roundedYPos = (ushort)trueYPos * -(short)finalscaleY & 0x7FF;
        height += trueYPos;
        trueYPos = 0;
    }

    if (width <= 0 || height <= 0)
        return;

    GFXSurface *surface    = &gfxSurface[sheetID];
    int pitch              = SCREEN_XSIZE - width;
    int gfxwidth           = surface->width;
    //byte *lineBuffer       = &gfxLineBuffer[trueYPos];
    byte *gfxData          = &graphicData[sprX + surface->width * sprY + surface->dataPosition];
    ushort *frameBufferPtr = &Engine.frameBuffer[trueXPos + SCREEN_XSIZE * trueYPos];
    if (direction == FLIP_X) {
        byte *gfxDataPtr = &gfxData[widthM1];
        int gfxPitch     = 0;
        while (height--) {
            int roundXPos = roundedXPos;
            int w         = width;
            while (w--) {
                if (*gfxDataPtr > 0)
                    *frameBufferPtr = tintLookupTable[*gfxDataPtr];
                int offsetX = finalscaleX + roundXPos;
                gfxDataPtr -= offsetX >> 11;
                gfxPitch += offsetX >> 11;
                roundXPos = offsetX & 0x7FF;
                ++frameBufferPtr;
            }
            frameBufferPtr += pitch;
            int offsetY = finalscaleY + roundedYPos;
            gfxDataPtr += gfxPitch + (offsetY >> 11) * gfxwidth;
            roundedYPos = offsetY & 0x7FF;
            gfxPitch    = 0;
        }
    }
    else {
        int gfxPitch = 0;
        int h        = height;
        while (h--) {
            int roundXPos = roundedXPos;
            int w         = width;
            while (w--) {
                if (*gfxData > 0)
                    *frameBufferPtr = tintLookupTable[*gfxData];
                int offsetX = finalscaleX + roundXPos;
                gfxData += offsetX >> 11;
                gfxPitch += offsetX >> 11;
                roundXPos = offsetX & 0x7FF;
                ++frameBufferPtr;
            }
            frameBufferPtr += pitch;
            int offsetY = finalscaleY + roundedYPos;
            gfxData += (offsetY >> 11) * gfxwidth - gfxPitch;
            roundedYPos = offsetY & 0x7FF;
            gfxPitch    = 0;
        }
    }
}

void DrawSprite(int XPos, int YPos, int width, int height, int sprX, int sprY, int sheetID)
{
    if (width + XPos > SCREEN_XSIZE)
        width = SCREEN_XSIZE - XPos;
    if (XPos < 0) {
        sprX -= XPos;
        width += XPos;
        XPos = 0;
    }
    if (height + YPos > SCREEN_YSIZE)
        height = SCREEN_YSIZE - YPos;
    if (YPos < 0) {
        sprY -= YPos;
        height += YPos;
        YPos = 0;
    }
    if (width <= 0 || height <= 0)
        return;

    GFXSurface *surface    = &gfxSurface[sheetID];
    int pitch              = SCREEN_XSIZE - width;
    int gfxPitch           = surface->width - width;
    byte *lineBuffer       = &gfxLineBuffer[YPos];
    byte *gfxDataPtr       = &graphicData[sprX + surface->width * sprY + surface->dataPosition];
    ushort *frameBufferPtr = &Engine.frameBuffer[XPos + SCREEN_XSIZE * YPos];
    while (height--) {
        activePalette   = fullPalette[*lineBuffer];
        activePalette32 = fullPalette32[*lineBuffer++];
        int w                    = width;
        while (w--) {
            if (*gfxDataPtr > 0)
                *frameBufferPtr = activePalette[*gfxDataPtr];
            ++gfxDataPtr;
            ++frameBufferPtr;
        }
        frameBufferPtr += pitch;
        gfxDataPtr += gfxPitch;
    }
}

void DrawSpriteFlipped(int XPos, int YPos, int width, int height, int sprX, int sprY, int direction, int sheetID)
{
    int widthFlip = width;
    int heightFlip = height;

    if (width + XPos > SCREEN_XSIZE) {
        width = SCREEN_XSIZE - XPos;
    }
    if (XPos < 0) {
        sprX -= XPos;
        width += XPos;
        widthFlip += XPos + XPos;
        XPos = 0;
    }
    if (height + YPos > SCREEN_YSIZE) {
        height = SCREEN_YSIZE - YPos;
    }
    if (YPos < 0) {
        sprY -= YPos;
        height += YPos;
        heightFlip += YPos + YPos;
        YPos = 0;
    }
    if (width <= 0 || height <= 0)
        return;

    GFXSurface *surface = &gfxSurface[sheetID];
    int pitch;
    int gfxPitch;
    byte *lineBuffer;
    byte *gfxData;
    ushort *frameBufferPtr;
    switch (direction) {
        case FLIP_NONE:
            pitch          = SCREEN_XSIZE - width;
            gfxPitch       = surface->width - width;
            lineBuffer     = &gfxLineBuffer[YPos];
            gfxData        = &graphicData[sprX + surface->width * sprY + surface->dataPosition];
            frameBufferPtr = &Engine.frameBuffer[XPos + SCREEN_XSIZE * YPos];

            while (height--) {
                activePalette = fullPalette[*lineBuffer++];
                int w                  = width;
                while (w--) {
                    if (*gfxData > 0)
                        *frameBufferPtr = activePalette[*gfxData];
                    ++gfxData;
                    ++frameBufferPtr;
                }
                frameBufferPtr += pitch;
                gfxData += gfxPitch;
            }
            break;
        case FLIP_X:
            pitch          = SCREEN_XSIZE - width;
            gfxPitch       = width + surface->width;
            lineBuffer     = &gfxLineBuffer[YPos];
            gfxData        = &graphicData[widthFlip - 1 + sprX + surface->width * sprY + surface->dataPosition];
            frameBufferPtr = &Engine.frameBuffer[XPos + SCREEN_XSIZE * YPos];
            while (height--) {
                activePalette = fullPalette[*lineBuffer++];
                int w                  = width;
                while (w--) {
                    if (*gfxData > 0)
                        *frameBufferPtr = activePalette[*gfxData];
                    --gfxData;
                    ++frameBufferPtr;
                }
                frameBufferPtr += pitch;
                gfxData += gfxPitch;
            }
            break;
        case FLIP_Y:
            pitch          = SCREEN_XSIZE - width;
            gfxPitch       = width + surface->width;
            lineBuffer     = &gfxLineBuffer[YPos];
            gfxData        = &gfxLineBuffer[YPos];
            gfxData        = &graphicData[sprX + surface->width * (sprY + heightFlip - 1) + surface->dataPosition];
            frameBufferPtr = &Engine.frameBuffer[XPos + SCREEN_XSIZE * YPos];
            while (height--) {
                activePalette = fullPalette[*lineBuffer++];
                int w                  = width;
                while (w--) {
                    if (*gfxData > 0)
                        *frameBufferPtr = activePalette[*gfxData];
                    ++gfxData;
                    ++frameBufferPtr;
                }
                frameBufferPtr += pitch;
                gfxData -= gfxPitch;
            }
            break;
        case FLIP_XY:
            pitch          = SCREEN_XSIZE - width;
            gfxPitch       = surface->width - width;
            lineBuffer     = &gfxLineBuffer[YPos];
            gfxData        = &graphicData[widthFlip - 1 + sprX + surface->width * (sprY + heightFlip - 1) + surface->dataPosition];
            frameBufferPtr = &Engine.frameBuffer[XPos + SCREEN_XSIZE * YPos];
            while (height--) {
                activePalette = fullPalette[*lineBuffer++];
                int w                  = width;
                while (w--) {
                    if (*gfxData > 0)
                        *frameBufferPtr = activePalette[*gfxData];
                    --gfxData;
                    ++frameBufferPtr;
                }
                frameBufferPtr += pitch;
                gfxData -= gfxPitch;
            }
            break;
        default: break;
    }
}
void DrawSpriteScaled(int direction, int XPos, int YPos, int pivotX, int pivotY, int scaleX, int scaleY, int width, int height, int sprX,
                               int sprY, int sheetID)
{
    int roundedYPos = 0;
    int roundedXPos = 0;
    int truescaleX  = 4 * scaleX;
    int truescaleY  = 4 * scaleY;
    int widthM1     = width - 1;
    int trueXPos    = XPos - (truescaleX * pivotX >> 11);
    width           = truescaleX * width >> 11;
    int trueYPos    = YPos - (truescaleY * pivotY >> 11);
    height          = truescaleY * height >> 11;
    int finalscaleX = (signed int)(float)((float)(2048.0 / (float)truescaleX) * 2048.0);
    int finalscaleY = (signed int)(float)((float)(2048.0 / (float)truescaleY) * 2048.0);
    if (width + trueXPos > SCREEN_XSIZE) {
        width = SCREEN_XSIZE - trueXPos;
    }

    if (direction) {
        if (trueXPos < 0) {
            widthM1 -= trueXPos * -finalscaleX >> 11;
            roundedXPos = (ushort)trueXPos * -(short)finalscaleX & 0x7FF;
            width += trueXPos;
            trueXPos = 0;
        }
    }
    else if (trueXPos < 0) {
        sprX += trueXPos * -finalscaleX >> 11;
        roundedXPos = (ushort)trueXPos * -(short)finalscaleX & 0x7FF;
        width += trueXPos;
        trueXPos = 0;
    }

    if (height + trueYPos > SCREEN_YSIZE) {
        height = SCREEN_YSIZE - trueYPos;
    }
    if (trueYPos < 0) {
        sprY += trueYPos * -finalscaleY >> 11;
        roundedYPos = (ushort)trueYPos * -(short)finalscaleY & 0x7FF;
        height += trueYPos;
        trueYPos = 0;
    }

    if (width <= 0 || height <= 0)
        return;

    GFXSurface *surface    = &gfxSurface[sheetID];
    int pitch              = SCREEN_XSIZE - width;
    int gfxwidth           = surface->width;
    byte *lineBuffer       = &gfxLineBuffer[trueYPos];
    byte *gfxData          = &graphicData[sprX + surface->width * sprY + surface->dataPosition];
    ushort *frameBufferPtr = &Engine.frameBuffer[trueXPos + SCREEN_XSIZE * trueYPos];
    if (direction == FLIP_X) {
        byte *gfxDataPtr = &gfxData[widthM1];
        int gfxPitch     = 0;
        while (height--) {
            activePalette = fullPalette[*lineBuffer++];
            int roundXPos          = roundedXPos;
            int w                  = width;
            while (w--) {
                if (*gfxDataPtr > 0)
                    *frameBufferPtr = activePalette[*gfxDataPtr];
                int offsetX = finalscaleX + roundXPos;
                gfxDataPtr -= offsetX >> 11;
                gfxPitch += offsetX >> 11;
                roundXPos = offsetX & 0x7FF;
                ++frameBufferPtr;
            }
            frameBufferPtr += pitch;
            int offsetY = finalscaleY + roundedYPos;
            gfxDataPtr += gfxPitch + (offsetY >> 11) * gfxwidth;
            roundedYPos = offsetY & 0x7FF;
            gfxPitch    = 0;
        }
    }
    else {
        int gfxPitch = 0;
        int h        = height;
        while (h--) {
            activePalette = fullPalette[*lineBuffer++];
            int roundXPos          = roundedXPos;
            int w                  = width;
            while (w--) {
                if (*gfxData > 0)
                    *frameBufferPtr = activePalette[*gfxData];
                int offsetX = finalscaleX + roundXPos;
                gfxData += offsetX >> 11;
                gfxPitch += offsetX >> 11;
                roundXPos = offsetX & 0x7FF;
                ++frameBufferPtr;
            }
            frameBufferPtr += pitch;
            int offsetY = finalscaleY + roundedYPos;
            gfxData += (offsetY >> 11) * gfxwidth - gfxPitch;
            roundedYPos = offsetY & 0x7FF;
            gfxPitch    = 0;
        }
    }
}
void DrawSpriteRotated(int direction, int XPos, int YPos, int pivotX, int pivotY, int sprX, int sprY, int width, int height, int rotation,
                                int sheetID)
{
    int sprXPos    = (pivotX + sprX) << 9;
    int sprYPos    = (pivotY + sprY) << 9;
    int fullwidth  = width + sprX;
    int fullheight = height + sprY;
    int angle      = rotation & 0x1FF;
    if (angle < 0)
        angle += 0x200;
    if (angle)
        angle = 0x200 - angle;
    int sine   = sinVal512[angle];
    int cosine = cosVal512[angle];
    int xPositions[4];
    int yPositions[4];

    if (direction == FLIP_X) {
        xPositions[0] = XPos + ((sine * (-pivotY - 2) + cosine * (pivotX + 2)) >> 9);
        yPositions[0] = YPos + ((cosine * (-pivotY - 2) - sine * (pivotX + 2)) >> 9);
        xPositions[1] = XPos + ((sine * (-pivotY - 2) + cosine * (pivotX - width - 2)) >> 9);
        yPositions[1] = YPos + ((cosine * (-pivotY - 2) - sine * (pivotX - width - 2)) >> 9);
        xPositions[2] = XPos + ((sine * (height - pivotY + 2) + cosine * (pivotX + 2)) >> 9);
        yPositions[2] = YPos + ((cosine * (height - pivotY + 2) - sine * (pivotX + 2)) >> 9);
        int a         = pivotX - width - 2;
        int b         = height - pivotY + 2;
        xPositions[3] = XPos + ((sine * b + cosine * a) >> 9);
        yPositions[3] = YPos + ((cosine * b - sine * a) >> 9);
    }
    else {
        xPositions[0] = XPos + ((sine * (-pivotY - 2) + cosine * (-pivotX - 2)) >> 9);
        yPositions[0] = YPos + ((cosine * (-pivotY - 2) - sine * (-pivotX - 2)) >> 9);
        xPositions[1] = XPos + ((sine * (-pivotY - 2) + cosine * (width - pivotX + 2)) >> 9);
        yPositions[1] = YPos + ((cosine * (-pivotY - 2) - sine * (width - pivotX + 2)) >> 9);
        xPositions[2] = XPos + ((sine * (height - pivotY + 2) + cosine * (-pivotX - 2)) >> 9);
        yPositions[2] = YPos + ((cosine * (height - pivotY + 2) - sine * (-pivotX - 2)) >> 9);
        int a         = width - pivotX + 2;
        int b         = height - pivotY + 2;
        xPositions[3] = XPos + ((sine * b + cosine * a) >> 9);
        yPositions[3] = YPos + ((cosine * b - sine * a) >> 9);
    }

    int left = SCREEN_XSIZE;
    for (int i = 0; i < 4; ++i) {
        if (xPositions[i] < left)
            left = xPositions[i];
    }
    if (left < 0)
        left = 0;

    int right = 0;
    for (int i = 0; i < 4; ++i) {
        if (xPositions[i] > right)
            right = xPositions[i];
    }
    if (right > SCREEN_XSIZE)
        right = SCREEN_XSIZE;
    int maxX = right - left;

    int top = SCREEN_YSIZE;
    for (int i = 0; i < 4; ++i) {
        if (yPositions[i] < top)
            top = yPositions[i];
    }
    if (top < 0)
        top = 0;

    int bottom = 0;
    for (int i = 0; i < 4; ++i) {
        if (yPositions[i] > bottom)
            bottom = yPositions[i];
    }
    if (bottom > SCREEN_YSIZE)
        bottom = SCREEN_YSIZE;
    int maxY = bottom - top;

    if (maxX <= 0 || maxY <= 0)
        return;

    GFXSurface *surface    = &gfxSurface[sheetID];
    int pitch              = SCREEN_XSIZE - maxX;
    int lineSize           = surface->widthShift;
    ushort *frameBufferPtr = &Engine.frameBuffer[left + SCREEN_XSIZE * top];
    byte *lineBuffer       = &gfxLineBuffer[top];
    int startX             = left - XPos;
    int startY             = top - YPos;
    int shiftPivot         = (sprX << 9) - 1;
    fullwidth <<= 9;
    int shiftheight = (sprY << 9) - 1;
    fullheight <<= 9;
    byte *gfxData = &graphicData[surface->dataPosition];
    if (cosine < 0 || sine < 0)
        sprYPos += sine + cosine;

    if (direction == FLIP_X) {
        int drawX = sprXPos - (cosine * startX - sine * startY) - 0x100;
        int drawY = cosine * startY + sprYPos + sine * startX;
        while (maxY--) {
            activePalette = fullPalette[*lineBuffer++];
            int finalX             = drawX;
            int finalY             = drawY;
            int w                  = maxX;
            while (w--) {
                if (finalX > shiftPivot && finalX < fullwidth && finalY > shiftheight && finalY < fullheight) {
                    byte index = gfxData[(finalY >> 9 << lineSize) + (finalX >> 9)];
                    if (index > 0)
                        *frameBufferPtr = activePalette[index];
                }
                ++frameBufferPtr;
                finalX -= cosine;
                finalY += sine;
            }
            drawX += sine;
            drawY += cosine;
            frameBufferPtr += pitch;
        }
    }
    else {
        int drawX = sprXPos + cosine * startX - sine * startY;
        int drawY = cosine * startY + sprYPos + sine * startX;
        while (maxY--) {
            activePalette = fullPalette[*lineBuffer++];
            int finalX             = drawX;
            int finalY             = drawY;
            int w                  = maxX;
            while (w--) {
                if (finalX > shiftPivot && finalX < fullwidth && finalY > shiftheight && finalY < fullheight) {
                    byte index = gfxData[(finalY >> 9 << lineSize) + (finalX >> 9)];
                    if (index > 0)
                        *frameBufferPtr = activePalette[index];
                }
                ++frameBufferPtr;
                finalX += cosine;
                finalY += sine;
            }
            drawX -= sine;
            drawY += cosine;
            frameBufferPtr += pitch;
        }
    }
}

void DrawSpriteRotozoom(int direction, int XPos, int YPos, int pivotX, int pivotY, int sprX, int sprY, int width, int height, int rotation,
                                 int scale, int sheetID)
{
    if (scale == 0)
        return;

    int sprXPos    = (pivotX + sprX) << 9;
    int sprYPos    = (pivotY + sprY) << 9;
    int fullwidth  = width + sprX;
    int fullheight = height + sprY;
    int angle      = rotation & 0x1FF;
    if (angle < 0)
        angle += 0x200;
    if (angle)
        angle = 0x200 - angle;
    int sine   = scale * sinVal512[angle] >> 9;
    int cosine = scale * cosVal512[angle] >> 9;
    int xPositions[4];
    int yPositions[4];

    if (direction == FLIP_X) {
        xPositions[0] = XPos + ((sine * (-pivotY - 2) + cosine * (pivotX + 2)) >> 9);
        yPositions[0] = YPos + ((cosine * (-pivotY - 2) - sine * (pivotX + 2)) >> 9);
        xPositions[1] = XPos + ((sine * (-pivotY - 2) + cosine * (pivotX - width - 2)) >> 9);
        yPositions[1] = YPos + ((cosine * (-pivotY - 2) - sine * (pivotX - width - 2)) >> 9);
        xPositions[2] = XPos + ((sine * (height - pivotY + 2) + cosine * (pivotX + 2)) >> 9);
        yPositions[2] = YPos + ((cosine * (height - pivotY + 2) - sine * (pivotX + 2)) >> 9);
        int a         = pivotX - width - 2;
        int b         = height - pivotY + 2;
        xPositions[3] = XPos + ((sine * b + cosine * a) >> 9);
        yPositions[3] = YPos + ((cosine * b - sine * a) >> 9);
    }
    else {
        xPositions[0] = XPos + ((sine * (-pivotY - 2) + cosine * (-pivotX - 2)) >> 9);
        yPositions[0] = YPos + ((cosine * (-pivotY - 2) - sine * (-pivotX - 2)) >> 9);
        xPositions[1] = XPos + ((sine * (-pivotY - 2) + cosine * (width - pivotX + 2)) >> 9);
        yPositions[1] = YPos + ((cosine * (-pivotY - 2) - sine * (width - pivotX + 2)) >> 9);
        xPositions[2] = XPos + ((sine * (height - pivotY + 2) + cosine * (-pivotX - 2)) >> 9);
        yPositions[2] = YPos + ((cosine * (height - pivotY + 2) - sine * (-pivotX - 2)) >> 9);
        int a         = width - pivotX + 2;
        int b         = height - pivotY + 2;
        xPositions[3] = XPos + ((sine * b + cosine * a) >> 9);
        yPositions[3] = YPos + ((cosine * b - sine * a) >> 9);
    }
    int truescale = (signed int)(float)((float)(512.0 / (float)scale) * 512.0);
    sine          = truescale * sinVal512[angle] >> 9;
    cosine        = truescale * cosVal512[angle] >> 9;

    int left = SCREEN_XSIZE;
    for (int i = 0; i < 4; ++i) {
        if (xPositions[i] < left)
            left = xPositions[i];
    }
    if (left < 0)
        left = 0;

    int right = 0;
    for (int i = 0; i < 4; ++i) {
        if (xPositions[i] > right)
            right = xPositions[i];
    }
    if (right > SCREEN_XSIZE)
        right = SCREEN_XSIZE;
    int maxX = right - left;

    int top = SCREEN_YSIZE;
    for (int i = 0; i < 4; ++i) {
        if (yPositions[i] < top)
            top = yPositions[i];
    }
    if (top < 0)
        top = 0;

    int bottom = 0;
    for (int i = 0; i < 4; ++i) {
        if (yPositions[i] > bottom)
            bottom = yPositions[i];
    }
    if (bottom > SCREEN_YSIZE)
        bottom = SCREEN_YSIZE;
    int maxY = bottom - top;

    if (maxX <= 0 || maxY <= 0)
        return;

    GFXSurface *surface    = &gfxSurface[sheetID];
    int pitch              = SCREEN_XSIZE - maxX;
    int lineSize           = surface->widthShift;
    ushort *frameBufferPtr = &Engine.frameBuffer[left + SCREEN_XSIZE * top];
    byte *lineBuffer       = &gfxLineBuffer[top];
    int startX             = left - XPos;
    int startY             = top - YPos;
    int shiftPivot         = (sprX << 9) - 1;
    fullwidth <<= 9;
    int shiftheight = (sprY << 9) - 1;
    fullheight <<= 9;
    byte *gfxData = &graphicData[surface->dataPosition];
    if (cosine < 0 || sine < 0)
        sprYPos += sine + cosine;

    if (direction == FLIP_X) {
        int drawX = sprXPos - (cosine * startX - sine * startY) - (truescale >> 1);
        int drawY = cosine * startY + sprYPos + sine * startX;
        while (maxY--) {
            activePalette = fullPalette[*lineBuffer++];
            int finalX             = drawX;
            int finalY             = drawY;
            int w                  = maxX;
            while (w--) {
                if (finalX > shiftPivot && finalX < fullwidth && finalY > shiftheight && finalY < fullheight) {
                    byte index = gfxData[(finalY >> 9 << lineSize) + (finalX >> 9)];
                    if (index > 0)
                        *frameBufferPtr = activePalette[index];
                }
                ++frameBufferPtr;
                finalX -= cosine;
                finalY += sine;
            }
            drawX += sine;
            drawY += cosine;
            frameBufferPtr += pitch;
        }
    }
    else {
        int drawX = sprXPos + cosine * startX - sine * startY;
        int drawY = cosine * startY + sprYPos + sine * startX;
        while (maxY--) {
            activePalette = fullPalette[*lineBuffer++];
            int finalX             = drawX;
            int finalY             = drawY;
            int w                  = maxX;
            while (w--) {
                if (finalX > shiftPivot && finalX < fullwidth && finalY > shiftheight && finalY < fullheight) {
                    byte index = gfxData[(finalY >> 9 << lineSize) + (finalX >> 9)];
                    if (index > 0)
                        *frameBufferPtr = activePalette[index];
                }
                ++frameBufferPtr;
                finalX += cosine;
                finalY += sine;
            }
            drawX -= sine;
            drawY += cosine;
            frameBufferPtr += pitch;
        }
    }
}

void DrawBlendedSprite(int XPos, int YPos, int width, int height, int sprX, int sprY, int sheetID)
{
    if (width + XPos > SCREEN_XSIZE)
        width = SCREEN_XSIZE - XPos;
    if (XPos < 0) {
        sprX -= XPos;
        width += XPos;
        XPos = 0;
    }
    if (height + YPos > SCREEN_YSIZE)
        height = SCREEN_YSIZE - YPos;
    if (YPos < 0) {
        sprY -= YPos;
        height += YPos;
        YPos = 0;
    }
    if (width <= 0 || height <= 0)
        return;

    GFXSurface *surface    = &gfxSurface[sheetID];
    int pitch              = SCREEN_XSIZE - width;
    int gfxPitch           = surface->width - width;
    byte *lineBuffer       = &gfxLineBuffer[YPos];
    byte *gfxData          = &graphicData[sprX + surface->width * sprY + surface->dataPosition];
    ushort *frameBufferPtr = &Engine.frameBuffer[XPos + SCREEN_XSIZE * YPos];
    while (height--) {
        activePalette = fullPalette[*lineBuffer++];
        int w                  = width;
        while (w--) {
            if (*gfxData > 0)
                *frameBufferPtr = ((activePalette[*gfxData] & 0xF7DE) >> 1) + ((*frameBufferPtr & 0xF7DE) >> 1);
            ++gfxData;
            ++frameBufferPtr;
        }
        frameBufferPtr += pitch;
        gfxData += gfxPitch;
    }
}
void DrawAlphaBlendedSprite(int XPos, int YPos, int width, int height, int sprX, int sprY, int alpha, int sheetID)
{
    if (width + XPos > SCREEN_XSIZE)
        width = SCREEN_XSIZE - XPos;
    if (XPos < 0) {
        sprX -= XPos;
        width += XPos;
        XPos = 0;
    }
    if (height + YPos > SCREEN_YSIZE)
        height = SCREEN_YSIZE - YPos;
    if (YPos < 0) {
        sprY -= YPos;
        height += YPos;
        YPos = 0;
    }
    if (width <= 0 || height <= 0 || alpha <= 0)
        return;

    if (alpha > 0xFF)
        alpha = 0xFF;
    GFXSurface *surface    = &gfxSurface[sheetID];
    int pitch              = SCREEN_XSIZE - width;
    int gfxPitch           = surface->width - width;
    byte *lineBuffer       = &gfxLineBuffer[YPos];
    byte *gfxData          = &graphicData[sprX + surface->width * sprY + surface->dataPosition];
    ushort *frameBufferPtr = &Engine.frameBuffer[XPos + SCREEN_XSIZE * YPos];
    if (alpha == 0xFF) {
        while (height--) {
            activePalette = fullPalette[*lineBuffer++];
            int w                  = width;
            while (w--) {
                if (*gfxData > 0)
                    *frameBufferPtr = activePalette[*gfxData];
                ++gfxData;
                ++frameBufferPtr;
            }
            frameBufferPtr += pitch;
            gfxData += gfxPitch;
        }
    }
    else {
        while (height--) {
            activePalette = fullPalette[*lineBuffer++];
            int w                  = width;
            while (w--) {
                if (*gfxData > 0) {
                    ushort colour         = activePalette[*gfxData];
                    short *blendTablePtrA = &blendLookupTable[BLENDTABLE_XSIZE * ((BLENDTABLE_YSIZE-1) - alpha)];
                    short *blendTablePtrB = &blendLookupTable[BLENDTABLE_XSIZE * alpha];
                    *frameBufferPtr = (blendTablePtrA[*frameBufferPtr & (BLENDTABLE_XSIZE - 1)] + blendTablePtrB[colour & (BLENDTABLE_XSIZE - 1)])
                                      | ((blendTablePtrA[(*frameBufferPtr & 0x7E0) >> 6] + blendTablePtrB[(colour & 0x7E0) >> 6]) << 6)
                                      | ((blendTablePtrA[(*frameBufferPtr & 0xF800) >> 11] + blendTablePtrB[(colour & 0xF800) >> 11]) << 11);
                }
                ++gfxData;
                ++frameBufferPtr;
            }
            frameBufferPtr += pitch;
            gfxData += gfxPitch;
        }
    }
}
void DrawAdditiveBlendedSprite(int XPos, int YPos, int width, int height, int sprX, int sprY, int alpha, int sheetID)
{
    if (width + XPos > SCREEN_XSIZE)
        width = SCREEN_XSIZE - XPos;
    if (XPos < 0) {
        sprX -= XPos;
        width += XPos;
        XPos = 0;
    }
    if (height + YPos > SCREEN_YSIZE)
        height = SCREEN_YSIZE - YPos;
    if (YPos < 0) {
        sprY -= YPos;
        height += YPos;
        YPos = 0;
    }
    if (width <= 0 || height <= 0 || alpha <= 0)
        return;

    if (alpha > 0xFF)
        alpha = 0xFF;

    short *blendTablePtr   = &blendLookupTable[BLENDTABLE_XSIZE * alpha];
    GFXSurface *surface    = &gfxSurface[sheetID];
    int pitch              = SCREEN_XSIZE - width;
    int gfxPitch           = surface->width - width;
    byte *lineBuffer       = &gfxLineBuffer[YPos];
    byte *gfxData          = &graphicData[sprX + surface->width * sprY + surface->dataPosition];
    ushort *frameBufferPtr = &Engine.frameBuffer[XPos + SCREEN_XSIZE * YPos];

    while (height--) {
        activePalette = fullPalette[*lineBuffer++];
        int w                  = width;
        while (w--) {
            if (*gfxData > 0) {
                ushort colour   = activePalette[*gfxData];
                int v20         = 0;
                int v21         = 0;
                int finalColour = 0;

                if (((ushort)blendTablePtr[(colour & 0xF800) >> 11] << 11) + (*frameBufferPtr & 0xF800) <= 0xF800)
                    v20 = ((ushort)blendTablePtr[(colour & 0xF800) >> 11] << 11) + (ushort)(*frameBufferPtr & 0xF800);
                else
                    v20 = 0xF800;
                int v12 = ((ushort)blendTablePtr[(colour & 0x7E0) >> 6] << 6) + (*frameBufferPtr & 0x7E0);
                if (v12 <= 0x7E0)
                    v21 = v12 | v20;
                else
                    v21 = v20 | 0x7E0;
                int v13 = (ushort)blendTablePtr[colour & (BLENDTABLE_XSIZE-1)] + (*frameBufferPtr & 0x1F);
                if (v13 <= 31)
                    finalColour = v13 | v21;
                else
                    finalColour = v21 | 0x1F;
                *frameBufferPtr = finalColour;
            }
            ++gfxData;
            ++frameBufferPtr;
        }
        frameBufferPtr += pitch;
        gfxData += gfxPitch;
    }
}
void DrawSubtractiveBlendedSprite(int XPos, int YPos, int width, int height, int sprX, int sprY, int alpha, int sheetID)
{
    if (width + XPos > SCREEN_XSIZE)
        width = SCREEN_XSIZE - XPos;
    if (XPos < 0) {
        sprX -= XPos;
        width += XPos;
        XPos = 0;
    }
    if (height + YPos > SCREEN_YSIZE)
        height = SCREEN_YSIZE - YPos;
    if (YPos < 0) {
        sprY -= YPos;
        height += YPos;
        YPos = 0;
    }
    if (width <= 0 || height <= 0 || alpha <= 0)
        return;

    if (alpha > 0xFF)
        alpha = 0xFF;

    short *subBlendTable   = &blendLookupTable[BLENDTABLE_XSIZE * alpha];
    GFXSurface *surface    = &gfxSurface[sheetID];
    int pitch              = SCREEN_XSIZE - width;
    int gfxPitch           = surface->width - width;
    byte *lineBuffer       = &gfxLineBuffer[YPos];
    byte *gfxData          = &graphicData[sprX + surface->width * sprY + surface->dataPosition];
    ushort *frameBufferPtr = &Engine.frameBuffer[XPos + SCREEN_XSIZE * YPos];

    while (height--) {
        activePalette = fullPalette[*lineBuffer++];
        int w                  = width;
        while (w--) {
            if (*gfxData > 0) {
                ushort colour      = activePalette[*gfxData];
                ushort finalColour = 0;
                if ((*frameBufferPtr & 0xF800) - ((ushort)subBlendTable[(colour & 0xF800) >> 11] << 11) <= 0)
                    finalColour = 0;
                else
                    finalColour = (ushort)(*frameBufferPtr & 0xF800) - ((ushort)subBlendTable[(colour & 0xF800) >> 11] << 11);
                int v12 = (*frameBufferPtr & 0x7E0) - ((ushort)subBlendTable[(colour & 0x7E0) >> 6] << 6);
                if (v12 > 0)
                    finalColour |= v12;
                int v13 = (*frameBufferPtr & (BLENDTABLE_XSIZE - 1)) - (ushort)subBlendTable[colour & (BLENDTABLE_XSIZE - 1)];
                if (v13 > 0)
                    finalColour |= v13;
                *frameBufferPtr = finalColour;
            }
            ++gfxData;
            ++frameBufferPtr;
        }
        frameBufferPtr += pitch;
        gfxData += gfxPitch;
    }
}

void DrawObjectAnimation(void *objScr, void *ent, int XPos, int YPos)
{
    ObjectScript *objectScript  = (ObjectScript *)objScr;
    Entity *entity              = (Entity *)ent;
    SpriteAnimation *sprAnim = &animationList[objectScript->animFile->aniListOffset + entity->animation];
    SpriteFrame *frame          = &animFrames[sprAnim->frameListOffset + entity->frame];
    int rotation                        = 0;

    switch (sprAnim->rotationFlag) {
        case ROTFLAG_NONE:
            switch (entity->direction) {
                case FLIP_NONE:
                    DrawSpriteFlipped(frame->pivotX + XPos, frame->pivotY + YPos, frame->width, frame->height, frame->sprX, frame->sprY,
                                               FLIP_NONE, frame->sheetID);
                    break;
                case FLIP_X:
                    DrawSpriteFlipped(XPos - frame->width - frame->pivotX, frame->pivotY + YPos, frame->width, frame->height, frame->sprX,
                                      frame->sprY, FLIP_X, frame->sheetID);
                    break;
                case FLIP_Y:
                    DrawSpriteFlipped(frame->pivotX + XPos, YPos - frame->height - frame->pivotY, frame->width, frame->height, frame->sprX,
                                      frame->sprY, FLIP_Y, frame->sheetID);
                    break;
                case FLIP_XY:
                    DrawSpriteFlipped(XPos - frame->width - frame->pivotX, YPos - frame->height - frame->pivotY, frame->width, frame->height,
                                      frame->sprX, frame->sprY, FLIP_XY, frame->sheetID);
                    break;
                default: break;
            }
            break;
        case ROTFLAG_FULL:
            DrawSpriteRotated(entity->direction, XPos, YPos, -frame->pivotX, -frame->pivotY, frame->sprX, frame->sprY, frame->width, frame->height,
                              entity->rotation, frame->sheetID);
            break;
        case ROTFLAG_45DEG:
            if (entity->rotation >= 0x100)
                DrawSpriteRotated(entity->direction, XPos, YPos, -frame->pivotX, -frame->pivotY, frame->sprX, frame->sprY, frame->width,
                                  frame->height, 0x200 - ((532 - entity->rotation) >> 6 << 6), frame->sheetID);
            else
                DrawSpriteRotated(entity->direction, XPos, YPos, -frame->pivotX, -frame->pivotY, frame->sprX, frame->sprX, frame->width,
                                  frame->height, (entity->rotation + 20) >> 6 << 6, frame->sheetID);
            break;
        case ROTFLAG_STATICFRAMES:
        {
            if (entity->rotation >= 0x100)
                rotation = 8 - ((532 - entity->rotation) >> 6);
            else
                rotation = (entity->rotation + 20) >> 6;
            int frameID = entity->frame;
            switch (rotation) {
                case 0: //0 deg
                case 8: // 360 deg
                    rotation = 0x00; break;
                case 1: //45 deg
                    frameID += sprAnim->frameCount;
                    if (entity->direction)
                        rotation = 0;
                    else
                        rotation = 0x80;
                    break;
                case 2: //90 deg
                    rotation = 0x80; break;
                case 3: //135 deg
                    frameID += sprAnim->frameCount;
                    if (entity->direction)
                        rotation = 0x80;
                    else
                        rotation = 0x100;
                    break;
                case 4: //180 deg
                    rotation = 0x100; break;
                case 5: //225 deg
                    frameID += sprAnim->frameCount;
                    if (entity->direction)
                        rotation = 0x100;
                    else
                        rotation = 384;
                    break;
                case 6: //270 deg
                    rotation = 384; break;
                case 7: //315 deg
                    frameID += sprAnim->frameCount;
                    if (entity->direction)
                        rotation = 384;
                    else
                        rotation = 0;
                    break;
                default: break;
            }

            frame = &animFrames[sprAnim->frameListOffset + frameID];
            DrawSpriteRotated(entity->direction, XPos, YPos, -frame->pivotX, -frame->pivotY, frame->sprX, frame->sprY, frame->width, frame->height,
                               rotation, frame->sheetID);
            //DrawSpriteRotozoom(entity->direction, XPos, YPos, -frame->pivotX, -frame->pivotY, frame->sprX, frame->sprY, frame->width, frame->height,
            //                  rotation, entity->scale, frame->sheetID);
            break;
        }
        default: break;
    }
}

void DrawFace(void *v, uint colour)
{
    Vertex *verts = (Vertex *)v;

    int alpha = (colour & 0x7F000000) >> 23;
    if (alpha < 1)
        return;
    if (alpha > 0xFF)
        alpha = 0xFF;
    if (verts[0].x < 0 && verts[1].x < 0 && verts[2].x < 0 && verts[3].x < 0)
        return;
    if (verts[0].x > SCREEN_XSIZE && verts[1].x > SCREEN_XSIZE && verts[2].x > SCREEN_XSIZE && verts[3].x > SCREEN_XSIZE)
        return;
    if (verts[0].y < 0 && verts[1].y < 0 && verts[2].y < 0 && verts[3].y < 0)
        return;
    if (verts[0].y > SCREEN_YSIZE && verts[1].y > SCREEN_YSIZE && verts[2].y > SCREEN_YSIZE && verts[3].y > SCREEN_YSIZE)
        return;
    if (verts[0].x == verts[1].x && verts[1].x == verts[2].x && verts[2].x == verts[3].x)
        return;
    if (verts[0].y == verts[1].y && verts[1].y == verts[2].y && verts[2].y == verts[3].y)
        return;

    int vertexA = 0;
    int vertexB = 1;
    int vertexC = 2;
    int vertexD = 3;
    if (verts[1].y < verts[0].y) {
        vertexA = 1;
        vertexB = 0;
    }
    if (verts[2].y < verts[vertexA].y) {
        int temp = vertexA;
        vertexA  = 2;
        vertexC  = temp;
    }
    if (verts[3].y < verts[vertexA].y) {
        int temp = vertexA;
        vertexA  = 3;
        vertexD  = temp;
    }
    if (verts[vertexC].y < verts[vertexB].y) {
        int temp = vertexB;
        vertexB  = vertexC;
        vertexC  = temp;
    }
    if (verts[vertexD].y < verts[vertexB].y) {
        int temp = vertexB;
        vertexB  = vertexD;
        vertexD  = temp;
    }
    if (verts[vertexD].y < verts[vertexC].y) {
        int temp = vertexC;
        vertexC  = vertexD;
        vertexD  = temp;
    }

    int faceTop    = verts[vertexA].y;
    int faceBottom = verts[vertexD].y;
    if (faceTop < 0)
        faceTop = 0;
    if (faceBottom > SCREEN_YSIZE)
        faceBottom = SCREEN_YSIZE;
    for (int i = faceTop; i < faceBottom; ++i) {
        faceLineStart[i] = 100000;
        faceLineEnd[i]   = -100000;
    }

    processScanEdge(&verts[vertexA], &verts[vertexB]);
    processScanEdge(&verts[vertexA], &verts[vertexC]);
    processScanEdge(&verts[vertexA], &verts[vertexD]);
    processScanEdge(&verts[vertexB], &verts[vertexC]);
    processScanEdge(&verts[vertexC], &verts[vertexD]);
    processScanEdge(&verts[vertexB], &verts[vertexD]);

    ushort colour16 = ((signed int)(byte)colour >> 3) | 32 * (((colour >> 8) & 0xFF) >> 2) | ((ushort)(((colour >> 16) & 0xFF) >> 3) << 11);

    ushort *frameBufferPtr = &Engine.frameBuffer[SCREEN_XSIZE * faceTop];
    if (alpha == 255) {
        while (faceTop < faceBottom) {
            int startX = faceLineStart[faceTop];
            int endX   = faceLineEnd[faceTop];
            if (startX >= SCREEN_XSIZE || endX <= 0) {
                frameBufferPtr += SCREEN_XSIZE;
            }
            else {
                if (startX < 0)
                    startX = 0;
                if (endX > SCREEN_XSIZE - 1)
                    endX = SCREEN_XSIZE - 1;
                ushort *fbPtr = &frameBufferPtr[startX];
                frameBufferPtr += SCREEN_XSIZE;
                int vertexwidth = endX - startX + 1;
                while (vertexwidth--) {
                    *fbPtr = colour16;
                    ++fbPtr;
                }
            }
            ++faceTop;
        }
    }
    else {
        while (faceTop < faceBottom) {
            int startX = faceLineStart[faceTop];
            int endX   = faceLineEnd[faceTop];
            if (startX >= SCREEN_XSIZE || endX <= 0) {
                frameBufferPtr += SCREEN_XSIZE;
            }
            else {
                if (startX < 0)
                    startX = 0;
                if (endX > SCREEN_XSIZE - 1)
                    endX = SCREEN_XSIZE - 1;
                ushort *fbPtr = &frameBufferPtr[startX];
                frameBufferPtr += SCREEN_XSIZE;
                int vertexwidth = endX - startX + 1;
                while (vertexwidth--) {
                    short *blendTableA = &blendLookupTable[BLENDTABLE_XSIZE * ((BLENDTABLE_YSIZE - 1) - alpha)];
                    short *blendTableB = &blendLookupTable[BLENDTABLE_XSIZE * alpha];
                    *fbPtr             = (blendTableA[*fbPtr & (BLENDTABLE_XSIZE - 1)] + blendTableB[colour16 & (BLENDTABLE_XSIZE - 1)])
                             | ((blendTableA[(*fbPtr & 0x7E0) >> 6] + blendTableB[(colour16 & 0x7E0) >> 6]) << 6)
                             | ((blendTableA[(*fbPtr & 0xF800) >> 11] + blendTableB[(colour16 & 0xF800) >> 11]) << 11);
                    ++fbPtr;
                }
            }
            ++faceTop;
        }
    }
}
void DrawTexturedFace(void *v, byte sheetID)
{
    Vertex *verts = (Vertex *)v;

    if (verts[0].x < 0 && verts[1].x < 0 && verts[2].x < 0 && verts[3].x < 0)
        return;
    if (verts[0].x > SCREEN_XSIZE && verts[1].x > SCREEN_XSIZE && verts[2].x > SCREEN_XSIZE && verts[3].x > SCREEN_XSIZE)
        return;
    if (verts[0].y < 0 && verts[1].y < 0 && verts[2].y < 0 && verts[3].y < 0)
        return;
    if (verts[0].y > SCREEN_YSIZE && verts[1].y > SCREEN_YSIZE && verts[2].y > SCREEN_YSIZE && verts[3].y > SCREEN_YSIZE)
        return;
    if (verts[0].x == verts[1].x && verts[1].x == verts[2].x && verts[2].x == verts[3].x)
        return;
    if (verts[0].y == verts[1].y && verts[1].y == verts[2].y && verts[2].y == verts[3].y)
        return;

    int vertexA = 0;
    int vertexB = 1;
    int vertexC = 2;
    int vertexD = 3;
    if (verts[1].y < verts[0].y) {
        vertexA = 1;
        vertexB = 0;
    }
    if (verts[2].y < verts[vertexA].y) {
        int temp = vertexA;
        vertexA  = 2;
        vertexC  = temp;
    }
    if (verts[3].y < verts[vertexA].y) {
        int temp = vertexA;
        vertexA  = 3;
        vertexD  = temp;
    }
    if (verts[vertexC].y < verts[vertexB].y) {
        int temp = vertexB;
        vertexB  = vertexC;
        vertexC  = temp;
    }
    if (verts[vertexD].y < verts[vertexB].y) {
        int temp = vertexB;
        vertexB  = vertexD;
        vertexD  = temp;
    }
    if (verts[vertexD].y < verts[vertexC].y) {
        int temp = vertexC;
        vertexC  = vertexD;
        vertexD  = temp;
    }

    int faceTop    = verts[vertexA].y;
    int faceBottom = verts[vertexD].y;
    if (faceTop < 0)
        faceTop = 0;
    if (faceBottom > SCREEN_YSIZE)
        faceBottom = SCREEN_YSIZE;
    for (int i = faceTop; i < faceBottom; ++i) {
        faceLineStart[i] = 100000;
        faceLineEnd[i]   = -100000;
    }

    processScanEdgeUV(&verts[vertexA], &verts[vertexB]);
    processScanEdgeUV(&verts[vertexA], &verts[vertexC]);
    processScanEdgeUV(&verts[vertexA], &verts[vertexD]);
    processScanEdgeUV(&verts[vertexB], &verts[vertexC]);
    processScanEdgeUV(&verts[vertexC], &verts[vertexD]);
    processScanEdgeUV(&verts[vertexB], &verts[vertexD]);

    ushort *frameBufferPtr = &Engine.frameBuffer[SCREEN_XSIZE * faceTop];
    byte *sheetPtr         = &graphicData[gfxSurface[sheetID].dataPosition];
    int shiftwidth         = gfxSurface[sheetID].widthShift;
    byte *lineBuffer       = &gfxLineBuffer[faceTop];
    while (faceTop < faceBottom) {
        activePalette = fullPalette[*lineBuffer++];
        int startX             = faceLineStart[faceTop];
        int endX               = faceLineEnd[faceTop];
        int UPos               = faceLineStartU[faceTop];
        int VPos               = faceLineStartV[faceTop];
        if (startX >= SCREEN_XSIZE || endX <= 0) {
            frameBufferPtr += SCREEN_XSIZE;
        }
        else {
            int posDifference = endX - startX;
            int bufferedUPos  = 0;
            int bufferedVPos  = 0;
            if (endX == startX) {
                bufferedUPos = 0;
                bufferedVPos = 0;
            }
            else {
                bufferedUPos = (faceLineEndU[faceTop] - UPos) / posDifference;
                bufferedVPos = (faceLineEndV[faceTop] - VPos) / posDifference;
            }
            if (endX > SCREEN_XSIZE - 1)
                posDifference = (SCREEN_XSIZE - 1) - startX;
            if (startX < 0) {
                posDifference += startX;
                UPos -= startX * bufferedUPos;
                VPos -= startX * bufferedVPos;
                startX = 0;
            }
            ushort *fbPtr = &frameBufferPtr[startX];
            frameBufferPtr += SCREEN_XSIZE;
            int counter = posDifference + 1;
            while (counter--) {
                if (UPos < 0)
                    UPos = 0;
                if (VPos < 0)
                    VPos = 0;
                ushort index = sheetPtr[(VPos >> 16 << shiftwidth) + (UPos >> 16)];
                if (index > 0)
                    *fbPtr = activePalette[index];
                fbPtr++;
                UPos += bufferedUPos;
                VPos += bufferedVPos;
            }
        }
        ++faceTop;
    }
}

void DrawBitmapText(void *menu, int XPos, int YPos, int scale, int spacing, int rowStart, int rowCount)
{
    TextMenu *tMenu = (TextMenu *)menu;
    int Y           = YPos << 9;
    if (rowCount < 0)
        rowCount = tMenu->rowCount;
    if (rowStart + rowCount > tMenu->rowCount) 
        rowCount = tMenu->rowCount - rowStart;

    while (rowCount > 0) {
        int charID = 0;
        int i      = tMenu->entrySize[rowStart];
        int X      = XPos << 9;
        while (i > 0) {
            char c               = tMenu->textData[tMenu->entryStart[rowStart] + charID];
            FontCharacter *fChar = &fontCharacterList[c];
            DrawSpriteScaled(FLIP_NONE, X >> 5, Y >> 5, -fChar->pivotX, -fChar->pivotY, scale, scale, fChar->width, fChar->height,
                                      fChar->srcX, fChar->srcY, textMenuSurfaceNo);
            X += fChar->xAdvance * scale;
            charID++;
            i--;
        }
        Y += spacing * scale;
        rowStart++;
        rowCount--;
    }
}

void DrawTextMenuEntry(void *menu, int rowID, int XPos, int YPos, int textHighlight)
{
    TextMenu *tMenu = (TextMenu *)menu;
    int id          = tMenu->entryStart[rowID];
    for (int i = 0; i < tMenu->entrySize[rowID]; ++i) {
        DrawSprite(XPos + (i << 3), YPos, 8, 8, (int)((int)(tMenu->textData[id] & 0xF) << 3),
                            (int)((int)(tMenu->textData[id] >> 4) << 3) + textHighlight, textMenuSurfaceNo);
        id++;
    }
}
void DrawStageTextEntry(void *menu, int rowID, int XPos, int YPos, int textHighlight)
{
    TextMenu *tMenu = (TextMenu *)menu;
    int id          = tMenu->entryStart[rowID];
    for (int i = 0; i < tMenu->entrySize[rowID]; ++i) {
        if (i == tMenu->entrySize[rowID] - 1) {
            DrawSprite(XPos + (i << 3), YPos, 8, 8, (int)((int)(tMenu->textData[id] & 0xF) << 3),
                                (int)((int)(tMenu->textData[id] >> 4) << 3), textMenuSurfaceNo);
        }
        else {
            DrawSprite(XPos + (i << 3), YPos, 8, 8, (int)((int)(tMenu->textData[id] & 0xF) << 3),
                                (int)((int)(tMenu->textData[id] >> 4) << 3) + textHighlight, textMenuSurfaceNo);
        }
        id++;
    }
}
void DrawBlendedTextMenuEntry(void *menu, int rowID, int XPos, int YPos, int textHighlight)
{
    TextMenu *tMenu = (TextMenu *)menu;
    int id          = tMenu->entryStart[rowID];
    for (int i = 0; i < tMenu->entrySize[rowID]; ++i) {
        DrawBlendedSprite(XPos + (i << 3), YPos, 8, 8, (int)((int)(tMenu->textData[id] & 0xF) << 3),
                                   (int)((int)(tMenu->textData[id] >> 4) << 3) + textHighlight, textMenuSurfaceNo);
        id++;
    }
}
void DrawTextMenu(void *menu, int XPos, int YPos)
{
    TextMenu *tMenu = (TextMenu *)menu;
    int cnt         = 0;
    if (tMenu->visibleRowCount > 0) {
        cnt = (int)(tMenu->visibleRowCount + tMenu->visibleRowOffset);
    }
    else {
        tMenu->visibleRowOffset = 0;
        cnt                     = (int)tMenu->rowCount;
    }
    if (tMenu->selectionCount == 3) {
        tMenu->selection2 = -1;
        for (int i = 0; i < tMenu->selection1 + 1; ++i) {
            if (tMenu->entryHighlight[i] == 1) {
                tMenu->selection2 = i;
            }
        }
    }
    switch (tMenu->alignment) {
        case 0:
            for (int i = (int)tMenu->visibleRowOffset; i < cnt; ++i) {
                switch (tMenu->selectionCount) {
                    case 1:
                        if (i == tMenu->selection1) 
                            DrawTextMenuEntry(tMenu, i, XPos, YPos, 128);
                        else 
                            DrawTextMenuEntry(tMenu, i, XPos, YPos, 0);
                        break;
                    case 2:
                        if (i == tMenu->selection1 || i == tMenu->selection2) 
                            DrawTextMenuEntry(tMenu, i, XPos, YPos, 128);
                        else 
                            DrawTextMenuEntry(tMenu, i, XPos, YPos, 0);
                        break;
                    case 3:
                        if (i == tMenu->selection1) 
                            DrawTextMenuEntry(tMenu, i, XPos, YPos, 128);
                        else 
                            DrawTextMenuEntry(tMenu, i, XPos, YPos, 0);
                        if (i == tMenu->selection2 && i != tMenu->selection1) 
                            DrawStageTextEntry(tMenu, i, XPos, YPos, 128);
                        break;
                }
                YPos += 8;
            }
            return;
        case 1:
            for (int i = (int)tMenu->visibleRowOffset; i < cnt; ++i) {
                int XPos2 = XPos - (tMenu->entrySize[i] << 3);
                switch (tMenu->selectionCount) {
                    case 1:
                        if (i == tMenu->selection1) 
                            DrawTextMenuEntry(tMenu, i, XPos2, YPos, 128);
                        else 
                            DrawTextMenuEntry(tMenu, i, XPos2, YPos, 0);
                        break;
                    case 2:
                        if (i == tMenu->selection1 || i == tMenu->selection2) 
                            DrawTextMenuEntry(tMenu, i, XPos2, YPos, 128);
                        else 
                            DrawTextMenuEntry(tMenu, i, XPos2, YPos, 0);
                        break;
                    case 3:
                        if (i == tMenu->selection1) 
                            DrawTextMenuEntry(tMenu, i, XPos2, YPos, 128);
                        else 
                            DrawTextMenuEntry(tMenu, i, XPos2, YPos, 0);
                        if (i == tMenu->selection2 && i != tMenu->selection1) 
                            DrawStageTextEntry(tMenu, i, XPos2, YPos, 128);
                        break;
                }
                YPos += 8;
            }
            return;
        case 2:
            for (int i = (int)tMenu->visibleRowOffset; i < cnt; ++i) {
                int XPos2 = XPos - (tMenu->entrySize[i] >> 1 << 3);
                switch (tMenu->selectionCount) {
                    case 1:
                        if (i == tMenu->selection1) 
                            DrawTextMenuEntry(tMenu, i, XPos2, YPos, 128);
                        else 
                            DrawTextMenuEntry(tMenu, i, XPos2, YPos, 0);
                        break;
                    case 2:
                        if (i == tMenu->selection1 || i == tMenu->selection2) 
                            DrawTextMenuEntry(tMenu, i, XPos2, YPos, 128);
                        else 
                            DrawTextMenuEntry(tMenu, i, XPos2, YPos, 0);
                        break;
                    case 3:
                        if (i == tMenu->selection1) 
                            DrawTextMenuEntry(tMenu, i, XPos2, YPos, 128);
                        else 
                            DrawTextMenuEntry(tMenu, i, XPos2, YPos, 0);

                        if (i == tMenu->selection2 && i != tMenu->selection1) 
                            DrawStageTextEntry(tMenu, i, XPos2, YPos, 128);
                        break;
                }
                YPos += 8;
            }
            return;
        default: return;
    }
}