
#ifndef METADATA_READER_H
#define METADATA_READER_H

#include <raylib.h>

#define MAX_LINES 50
#define LINE_PADDING 30
#define MARGIN_TOP 70
#define MARGIN_BOTTOM 40

typedef struct {
    char text[1024];
    Vector2 position;
    Color color;
    float fontSize;
} TextLine;

void readMetadataFile(const char* outputFile);
void calculateTextPositions(TextLine* lines, int lineCount, int screenWidth, int screenHeight);
void drawMetadataText(Font font, TextLine* lines, int lineCount);


#endif 
