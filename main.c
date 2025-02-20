#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <raylib.h>
#include <raymath.h>
#include <string.h>
#include <unistd.h>
#include <linux/limits.h>

#include "metadata_reader.h"

#define WIDTH 1200
#define HEIGHT 800
#define FPS 60

#define COLOR_WHITE (Color){ 255, 255, 255, 255 }
#define COLOR_YELLOW (Color){ 253, 249, 0, 255 }

#define PROGRESS_BAR_WIDTH 800
#define PROGRESS_BAR_HEIGHT 20
#define PROGRESS_BAR_Y 700
#define PROGRESS_BAR_X (WIDTH - PROGRESS_BAR_WIDTH) / 2.0

Color headDotColor = { 255, 255, 255, 255 };
TextLine lines[MAX_LINES];
int g_lineCount = 0; // g for global

struct Metadata {
    const char* title;
    const char* album;
    const char* date;
    const char* artist;

};

void getInforGuide(Music *music)
{
    if (IsMusicStreamPlaying(*music)) {
        DrawText("Press space to stop music", 50, 50, 20, RED);
    } else {
        DrawText("Press space to play music", 50, 50, 20, RED);
    }
}

void formatMusicTime(float seconds, char* buffer, size_t bufferSize) {
    int minutes = (int)seconds / 60;
    int secs = (int)seconds % 60;
    snprintf(buffer, bufferSize,  "%02d:%02d", minutes, secs);
}

void toggleHeadDotColor() 
{
    if (headDotColor.r == COLOR_WHITE.r && headDotColor.g == COLOR_WHITE.g && headDotColor.b == COLOR_WHITE.b) {
        headDotColor = COLOR_YELLOW;
    } else {
        headDotColor = COLOR_WHITE;
    }
}

void handleProgressBarClick(Music *music, Vector2 mousePosition)
{
    Rectangle rec = { PROGRESS_BAR_X, PROGRESS_BAR_Y, PROGRESS_BAR_WIDTH, PROGRESS_BAR_HEIGHT, LIGHTGRAY };
    if (CheckCollisionPointRec(mousePosition, rec)  && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        float clickPosition = (mousePosition.x - PROGRESS_BAR_X) / PROGRESS_BAR_WIDTH;
        float newTime = clickPosition * GetMusicTimeLength(*music);
        SeekMusicStream(*music, newTime);
    }
}

void drawProgressBar(Music *music) 
{
    float timeLength = GetMusicTimeLength(*music);
    float timePlayed = GetMusicTimePlayed(*music);
    float progress = timePlayed / timeLength;

    char formattedTimePlayed[32];
    char formattedTimeLength[32];

    formatMusicTime(timePlayed, formattedTimePlayed, sizeof(formattedTimePlayed));
    formatMusicTime(timeLength, formattedTimeLength, sizeof(formattedTimeLength));

    DrawRectangle(PROGRESS_BAR_X, PROGRESS_BAR_Y, PROGRESS_BAR_WIDTH, PROGRESS_BAR_HEIGHT, LIGHTGRAY);
    DrawRectangle(PROGRESS_BAR_X, PROGRESS_BAR_Y, PROGRESS_BAR_WIDTH * progress, PROGRESS_BAR_HEIGHT, MAROON);

    DrawCircle(PROGRESS_BAR_X + PROGRESS_BAR_WIDTH * progress, 
            PROGRESS_BAR_Y + 9, 
            15, 
            headDotColor);
    /* if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) { */

    /* } */

    DrawText(TextFormat("%s / %s", 
                formattedTimePlayed, 
                formattedTimeLength), 
                WIDTH / 2 - 50, 
                PROGRESS_BAR_Y + 40, 
                20, 
                RAYWHITE);

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        Vector2 mousePosition = GetMousePosition();
        Vector2 circleCenter = (Vector2) { PROGRESS_BAR_X + PROGRESS_BAR_WIDTH * progress, PROGRESS_BAR_Y + 9 };
        int circleRadius = 15;
        float distance = Vector2Distance(mousePosition, circleCenter);
        if (distance <= circleRadius) {
            toggleHeadDotColor();
        }
    } 
}

void toggleMusic(Music *music)
{
    if (IsKeyPressed(KEY_SPACE)) {
        if (IsMusicStreamPlaying(*music)) {
            PauseMusicStream(*music);
        } else {
            if (!IsMusicStreamPlaying(*music)) {
                PlayMusicStream(*music);
            } else {
                ResumeMusicStream(*music);
            }
        }
    }
}

void readMetadataFile(const char* outputFile)
{
    if (access(outputFile , F_OK) == -1) {
        const char* errorMsg = "Cannot read metadata file";
        int errorWidth = MeasureText(errorMsg, 20);
        DrawText(errorMsg, (GetScreenWidth() - errorWidth) / 2 , 20, 20, RED);
        return;
    } 

    FILE *file = fopen(outputFile, "r"); // Let's read this file
    if (!file) {
        const char* errorMsg = "Error opening metadata file";
        int errorWidth = MeasureText(errorMsg, 20);
        DrawText(errorMsg, (GetScreenWidth() - errorWidth) / 2 , 20, 20, RED);
        return;
    }

    int i = 0;
    while (i < MAX_LINES && fgets(lines[i].text, sizeof(lines[i].text), file)) {
        size_t len = strlen(lines[i].text);
        // If it is the end of the string
        if (len > 0 && lines[i].text[len - 1] == '\n') {
            lines[i].text[len - 1] = '\0'; // Terminate
        }
        lines[i].fontSize = 20;
        lines[i].color = WHITE;
        i ++;
    }

    calculateTextPositions(lines, i, GetScreenWidth(), GetScreenHeight());

    fclose(file);
}

void calculateTextPositions(TextLine *lines, int lineCount, int screenWidth, int screenHeight)
{
    float totalHeight = lineCount * LINE_PADDING;
    float startY = MARGIN_TOP;

    if (totalHeight > (screenHeight - MARGIN_TOP - MARGIN_BOTTOM)) {
        startY = screenHeight - MARGIN_TOP - (lineCount * LINE_PADDING);
    }

    for (int i = 0; i < lineCount; i++) {
        int widthText = MeasureText(lines[i].text, 20);
        lines[i].position.x = (screenWidth - widthText) / 2.0;
        lines[i].position.y = startY + (i * LINE_PADDING);
        if (strstr(lines[i].text, "title") != NULL) {
            lines[i].color = GOLD;
            lines[i].fontSize = 24;
        }
    }
    g_lineCount = lineCount;
}

void drawMetadataText(TextLine *lines, int lineCount)
{
    for (int i = 0; i < lineCount; i++) {
        DrawText(lines[i].text, lines[i].position.x, 
                lines[i].position.y, 
                lines[i].fontSize, lines[i].color);
    }
}

int executeCommand(const char *inputFile, const char* outputFile)
{
    // 0 if the file exists, -1 if it is not
    if (access(outputFile, F_OK) == 0) {
        if (remove(outputFile) != 0) {
            perror("Error removing existing output file");
            return -1;
        } else {
            // Remove the file if it exists
            printf("File deleted successfully.\n");
        }
    }

    // If the file doesn't exist, execute the command below
    char command[256];

    printf("Input file: %s\n", inputFile);
    printf("Output file: %s\n", outputFile);

    snprintf(command, sizeof(command), "ffmpeg -i %s -f ffmetadata %s", inputFile, outputFile);

    int result = system(command);
    if (result == 0) {
        printf("Exetracted metadata successfully");
        return 0;
    } else {
        printf("Failed to exetracted metadata successfully");
        return -1;
    }
}

static void AudioProcessEffectLPF(void *buffer, unsigned int frames)
{
    static float low[2] = { 0.0f, 0.0f };
    static const float cutoff = 70.0f / 44100.0f; // 70 Hz lowpass filter
    const float k = cutoff / (cutoff + 0.1591549431f); // RC filter formula

    // Converts the buffer data before using it
    float *bufferData = (float *)buffer;
    for (unsigned int i = 0; i < frames*2; i += 2)
    {
        const float l = bufferData[i];
        const float r = bufferData[i + 1];

        low[0] += k * (l - low[0]);
        low[1] += k * (r - low[1]);
        bufferData[i] = low[0];
        bufferData[i + 1] = low[1];
    }
}

int main()
{
    InitAudioDevice();
    InitWindow(WIDTH, HEIGHT, "Play with sound");
    SetTargetFPS(FPS);

    Music music = LoadMusicStream("resources/journey-dont-stop-believin-1981.mp3");

    char cwd[PATH_MAX];

    const char* fileName = GetFileName("/home/chinhcom/programming/c/resources/journey-dont-stop-believin-1981.mp3");

    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("The current path: %s\n", cwd);
    } else {
        printf("Cannot get the current path\n");
    }

    char* filePath = strcat(cwd, "/resources/");
    strcat(filePath, fileName);

    const char* outputFile = "metadata.txt";

    if (executeCommand(filePath, outputFile) == 0) {
        readMetadataFile(outputFile);
    }

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(BLACK);

        if (!IsMusicValid(music)) {
            DrawText("Cannot playing music", 100, 100, 20, RED);
        } else {
            UpdateMusicStream(music);
            toggleMusic(&music);
            getInforGuide(&music);
            drawProgressBar(&music);
            handleProgressBarClick(&music, GetMousePosition());
            drawMetadataText(lines, g_lineCount);
        }

        if (IsKeyPressed(KEY_F)) {
            AttachAudioStreamProcessor(music.stream, AudioProcessEffectLPF);
        }

        EndDrawing();
    }
    UnloadMusicStream(music);
    CloseAudioDevice();
    CloseWindow();
    return 0;
}
