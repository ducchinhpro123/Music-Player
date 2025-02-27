#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <raylib.h>
#include <raymath.h>
#include <string.h>
#include <unistd.h>
#include <linux/limits.h>
#include <stdint.h>
#include <complex.h>
/* #include <kissfft/kiss_fft.h> */

#include "metadata_reader.h"

#define WIDTH 1200
#define HEIGHT 800
#define FPS 60
#define FRAME_BUFFER 4024
#define N (1<<13)

#define FFT_SIZE 1024
#define NUM_BARS 64

#define ARRAY_LEN(x) sizeof(x) / sizeof(x[0])

#define COLOR_WHITE (Color){ 255, 255, 255, 255 }
#define COLOR_YELLOW (Color){ 253, 249, 0, 255 }

#define PROGRESS_BAR_WIDTH 800
#define PROGRESS_BAR_HEIGHT 20
#define BAR_WIDTH 20
#define PROGRESS_BAR_Y 700
#define PROGRESS_BAR_X (WIDTH - PROGRESS_BAR_WIDTH) / 2.0

struct Metadata {
    const char* title;
    const char* album;
    const char* date;
    const char* artist;
};

typedef struct {
    float left;
    float right;
} Frame;

Color headDotColor = { 255, 255, 255, 255 };
TextLine lines[MAX_LINES];

int g_lineCount = 0; 

Frame g_frames[FRAME_BUFFER] = { 0 };
size_t g_frames_count = 0;

float global_time;
float fft_in[FFT_SIZE];
float fft_out[FFT_SIZE];

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

void handleSeekMusic(Music *music)
{
    if (IsKeyPressed(KEY_RIGHT)) {
        float timePlayed = GetMusicTimePlayed(*music);
        timePlayed += 5;
        SeekMusicStream(*music, timePlayed);
    }
    if (IsKeyPressed(KEY_LEFT)) {
        float timePlayed = GetMusicTimePlayed(*music);
        timePlayed -= 5;
        if (timePlayed <= 0) {
            SeekMusicStream(*music, 0);
        } else {
            SeekMusicStream(*music, timePlayed);
        }
    }
}

void handleProgressBarClick(Music *music, Vector2 mousePosition)
{
    Rectangle rec = { PROGRESS_BAR_X, PROGRESS_BAR_Y, PROGRESS_BAR_WIDTH, PROGRESS_BAR_HEIGHT };
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

    /* DrawRectangle(int posX, int posY, int width, int height, Color color); */
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
        if (strncmp(lines[i].text, ";FFMETADATA1", 12) == 0
                || strncmp(lines[i].text, "comment=(myzuka)", 16) == 0) {
            continue;
        }
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
            lines[i].fontSize = 30;
        }
    }
    g_lineCount = lineCount;
}

void drawMetadataText(Font font, TextLine *lines, int lineCount)
{
    for (int i = 0; i < lineCount; i++) {
        DrawTextEx(font, lines[i].text, 
                (Vector2) { lines[i].position.x, lines[i].position.y },
                lines[i].fontSize, 2, lines[i].color);
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
        printf("Exetracted metadata successfully\n");
        return 0;
    } else {
        printf("Failed to exetracted metadata successfully\n");
        return -1;
    }
}

Color getRainbowColor()
{
    const float r = sinf(global_time);
    const float g = sinf(global_time + 0.33f * 2.0f * PI);
    const float b = sinf(global_time + 0.66f * 2.0f * PI);

    return (Color){
        (unsigned char)(255.0f * r * r), 
        (unsigned char)(255.0f * g * g), 
        (unsigned char)(255.0f * b * b), 
        255.0f};
}

// I copy from tsoding
void drawWaveform(Frame *frames, size_t count)
{
    if (count == 0) return;

    int w = GetRenderWidth();
    int h = GetRenderHeight();
    float cellWidth = (float)w/count; // normalize this 

    int x = 0;
    for (size_t i = 0; i < count; i++) {
        float t = g_frames[i].left;
        // Trying to visualize that sample
        if (t > 0) {
            /* DrawRectangle(int posX, int posY, int width, int height, Color color); */
            /* DrawRectangle(i*cellWidth, h/2.0 - h/2.0*t, 1, h/2.0*t, getRainbowColor()); */
            DrawRectangle(x, h/2.0 - h/2.0*t, BAR_WIDTH, h/2.0*t, getRainbowColor());
        } else {
            /* DrawRectangle(i*cellWidth, h/2            , 1, h/2.0*t, getRainbowColor()); */
            /* DrawRectangle(x, 200, BAR_WIDTH,  h/2.0*t, getRainbowColor()); */
            DrawRectangle(x, h/2.0, BAR_WIDTH, h/2.0*t, getRainbowColor());
        }
        x += BAR_WIDTH + 1;
    }
}

// I copy from tsoding
void callback(void *bufferData, unsigned int frames)
{
    size_t capacity = ARRAY_LEN(g_frames);
    if (frames <= capacity - g_frames_count) {
        memcpy(g_frames + g_frames_count, bufferData, sizeof(Frame)*frames);
        g_frames_count += frames;
    } else if (frames <= capacity) {
        memmove(g_frames, g_frames+frames, sizeof(Frame)*(capacity - frames));
        memcpy(g_frames+(capacity-frames), bufferData, sizeof(Frame)*frames);
    } else {
        memcpy(g_frames, bufferData, sizeof(Frame)*capacity);
        g_frames_count = capacity;
    }
}

int main()
{
    InitAudioDevice();
    InitWindow(WIDTH, HEIGHT, "Play with sound");
    SetTargetFPS(FPS);
    SetExitKey(KEY_ESCAPE);

    Music music = LoadMusicStream("resources/journey-dont-stop-believin-1981.mp3");
    Font font = LoadFont("resources/DejaVuSans.ttf");
    /* INFO:     > Sample rate:   44100 Hz */
    /* INFO:     > Sample size:   32 bits */
    /* INFO:     > Channels:      2 (Stereo) */
    /* INFO:     > Total frames:  11052288 */

    char cwd[PATH_MAX];
    const char* fileName = GetFileName("journey-dont-stop-believin-1981.mp3");
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("The current path: %s\n", cwd);
    } else {
        printf("Cannot get the current path\n");
    }

    char* filePath = strcat(cwd, "/resources/");
    printf("file_path: %s \n", filePath);
    printf("file_name: %s \n", fileName);
    strcat(filePath, fileName);

    const char* outputFile = "metadata.txt";

    if (executeCommand(filePath, outputFile) == 0) {
        readMetadataFile(outputFile);
    }
    AttachAudioStreamProcessor(music.stream, callback);

    while (!WindowShouldClose()) {
        float delta_time = GetFrameTime();
        global_time += delta_time;

        BeginDrawing();
        ClearBackground(BLACK);

        DrawFPS(10, 10);

        if (!IsMusicValid(music)) {
            DrawText("Cannot playing music", 100, 100, 20, RED);
        } else {
            UpdateMusicStream(music);

            toggleMusic(&music);
            getInforGuide(&music);
            drawProgressBar(&music);
            handleProgressBarClick(&music, GetMousePosition());
            drawMetadataText(font, lines, g_lineCount);
            handleSeekMusic(&music);

            drawWaveform(g_frames, g_frames_count);
        }
        EndDrawing();
    }
    UnloadFont(font);
    UnloadMusicStream(music);
    CloseAudioDevice();
    CloseWindow();
    return 0;
}
