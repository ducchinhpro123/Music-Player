#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <raylib.h>
#include <raymath.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <linux/limits.h>
#include <stdint.h>
#include <complex.h>
#include <assert.h>
/* #include <kissfft/kiss_fft.h> */
#include <rlgl.h>

#include "metadata_reader.h"

#define WIDTH 1200
#define HEIGHT 800
#define FPS 60
#define FRAME_BUFFER 4024
#define N (1<<13)

#define FFT_SIZE 1024
#define NUM_BARS 128

#define ARRAY_LEN(x) sizeof(x) / sizeof(x[0])

#define COLOR_WHITE (Color){ 255, 255, 255, 255 }
#define COLOR_YELLOW (Color){ 253, 249, 0, 255 }

#define PROGRESS_BAR_WIDTH 800
#define PROGRESS_BAR_HEIGHT 20
#define BAR_WIDTH 20
#define PROGRESS_BAR_Y 700
#define PROGRESS_BAR_X (WIDTH - PROGRESS_BAR_WIDTH) / 2.0

#define MAX_SAMPLES 512
#define MAX_SAMPLES_PER_UPDATE 4096

#define MAX_FILEPATH_RECORDED   4096
#define MAX_FILEPATH_SIZE       2048

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
/* float fft_out[FFT_SIZE]; */
float complex fft_out[FFT_SIZE];
float fft_magnitudes[FFT_SIZE/2];

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

    snprintf(command, sizeof(command), "ffmpeg -i '%s' -f ffmetadata %s", inputFile, outputFile);

    int result = system(command);
    if (result == 0) {
        printf("Exetracted metadata successfully\n");
        return 0;
    } else {
        printf("Failed to exetracted metadata successfully\n");
        return -1;
    }
}

// Copy from Claude 3.7
Color getRainbowColor(float offset)
{
    float position = fmodf(global_time * 0.2f + offset, 1.0f);
    
    // Simple HSV to RGB conversion
    float h = position * 360.0f;
    float s = 1.0f;
    float v = 1.0f;
    
    float c = v * s;
    float x = c * (1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f));
    float m = v - c;
    
    float r, g, b;
    
    if (h < 60.0f) { r = c; g = x; b = 0; }
    else if (h < 120.0f) { r = x; g = c; b = 0; }
    else if (h < 180.0f) { r = 0; g = c; b = x; }
    else if (h < 240.0f) { r = 0; g = x; b = c; }
    else if (h < 300.0f) { r = x; g = 0; b = c; }
    else { r = c; g = 0; b = x; }
    
    return (Color){
        (unsigned char)((r + m) * 255),
        (unsigned char)((g + m) * 255),
        (unsigned char)((b + m) * 255),
        255
    };
}

void applyHannWindow(float* input, int size) {
    for (int i = 0; i < size; i++) {
        // Hann window: 0.5 * (1 - cos(2π*n/(N-1)))
        float window = 0.5f * (1.0f - cosf(2.0f * PI * i / (size - 1)));
        input[i] *= window;
    }
}

void fft(float in[], size_t stride, float complex out[], size_t n)
{
    assert(n > 0);

    if (n == 1) {
        out[0] = in[0];
        return;
    }

    fft(in, stride*2, out, n/2);
    fft(in + stride, stride*2, out + n/2, n/2);

    for (size_t k = 0; k < n/2; ++k) {
        float t = (float)k/n;
        float complex v = cexp(-2*I*PI*t)*out[k + n/2];
        float complex e = out[k];
        out[k]       = e + v;
        out[k + n/2] = e - v;
    }
}

// Calculate FFT and store magnitudes
void computeFFTSpectrum() {
    // Make a copy of the audio data
    float windowedInput[FFT_SIZE];
    
    // Cap to the available frames
    int samplesToCopy = FFT_SIZE;
    if (g_frames_count < FFT_SIZE) {
        samplesToCopy = g_frames_count;
    }
    
    // Extract audio samples from frames
    for (int i = 0; i < samplesToCopy; i++) {
        windowedInput[i] = g_frames[i].left;
    }
    
    // Zero-pad if we have fewer samples than FFT size
    if (samplesToCopy < FFT_SIZE) {
        memset(windowedInput + samplesToCopy, 0, (FFT_SIZE - samplesToCopy) * sizeof(float));
    }
    
    // Apply window function to reduce spectral leakage
    applyHannWindow(windowedInput, FFT_SIZE);
    
    // Compute FFT
    fft(windowedInput, 1, fft_out, FFT_SIZE);
    
    // Calculate magnitude spectrum (only need first half due to symmetry)
    for (int i = 0; i < FFT_SIZE/2; i++) {
        // Get magnitude of complex number (|z| = sqrt(re^2 + im^2))
        float magnitude = cabsf(fft_out[i]);
        
        // Convert to decibels with some scaling and offset
        fft_magnitudes[i] = 20.0f * log10f(magnitude + 1e-6f);
    }
}

// Ask Claude 3.7
void drawFFTSpectrum(complex float* fftData, int size) {
    if (size == 0) return;
    
    int w = GetRenderWidth();
    int h = GetRenderHeight();
    int barCount = 64;  
    float barWidth = (float)w/barCount;

    computeFFTSpectrum();
    static float prevHeights[64] = {0};
    
    // Process audio data directly from frames
    for (int i = 0; i < FFT_SIZE && i < g_frames_count; i++) {
        // Just use the audio samples directly - no complex FFT calculation
        fft_in[i] = fabsf(g_frames[i].left); // Use absolute values
    }
    
    // Simple visualization without trying to do complex FFT
    for (int i = 0; i < barCount; i++) {
        // Sample range of audio data for this bar
        int lowBound = i * FFT_SIZE / barCount;
        int hiBound = (i + 1) * FFT_SIZE / barCount;
        if (hiBound > FFT_SIZE) hiBound = FFT_SIZE;
        
        // Find max amplitude in this range
        float max = 0.0f;
        for (int j = lowBound; j < hiBound && j < FFT_SIZE; j++) {
            if (fft_in[j] > max) max = fft_in[j];
        }
        
        // Amplify for better visualization - raylib audio data is typically normalized between -1.0 and 1.0
        float barHeight = max * h * 1.1f; // Amplify by 5x

        float riseSpeed = 0.35f;    // Faster response to volume increases
        float fallSpeed = 0.08f;    // Slower decay for smoother falling motion
        
        
        // Apply smoothing with previous frame values
        float smoothFactor = 0.15f;
        float smoothedHeight = prevHeights[i] * (1.0f - smoothFactor) + barHeight * smoothFactor;
        prevHeights[i] = smoothedHeight;
        
        // Draw the bar
        Color barColor = getRainbowColor(i * 0.05f);
        /* DrawRectangle(i * barWidth, h/2.0 - smoothedHeight/2, barWidth-2, smoothedHeight, barColor); */
        /* DrawRectangle(int posX, int posY, int width, int height, Color color) */
        float x_pos = i * barWidth;
        DrawRectangle(x_pos, h/2.0 - smoothedHeight, barWidth-2, smoothedHeight, barColor);

        Color reflectionColor = barColor;
        reflectionColor.a = 100; // Slightly transparent reflection
        DrawRectangle(x_pos, h/2.0, barWidth-2, smoothedHeight - 40, reflectionColor);
    }

    DrawLine(0, h/2, w, h/2, ColorAlpha(WHITE, 0.6f));
}

void computeFFT(float* input, complex float* output, int size)
{
    for (int k = 0; k < size/2; k++) {
        float real = 0.0f;
        float imag = 0.0f;
        
        for (int n = 0; n < size; n++) {
            float angle = 2.0f * PI * k * n / size;
            real += input[n] * cosf(angle);
            imag += input[n] * sinf(angle);
        }
        
        // Calculate magnitude
        output[k] = 10.0f * log10f(sqrtf(real*real + imag*imag) + 1e-6f);
    }
}

void drawWaveform(Frame *frames, size_t count)
{
    if (count == 0) return;

    int w = GetRenderWidth();
    int h = GetRenderHeight();

    for (int i = 0; i < FFT_SIZE && i < count; i++) {
        fft_in[i] = frames[i].left;
    }
    
    computeFFT(fft_in, fft_out, FFT_SIZE);
    float cellWidth = (float)w/count; // normalize this 
                                      
    float xStep = (float)w / (count / 2.0);
    float x = 0;

    drawFFTSpectrum(fft_out, FFT_SIZE);
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

bool isAudioFile(const char* file_path) {
    const char *extensions[] = {".mp3", ".wav", ".ogg", ".aac", ".wma"};
    const int numExtensions = sizeof(extensions) / sizeof(extensions[0]);

    const char *extension = strrchr(file_path, '.');
    if (!extension) return false;
    for (int i = 0; i < numExtensions; i++) {
        if (strcasecmp(extension, extensions[i]) == 0) {
            return true;
        }
    }

    return false; 
}

// Copy from tsodoing
bool IsMusicReady(Music music)
{
  return ((music.ctxData != NULL) &&          // Validate context loaded
            (music.frameCount > 0) &&           // Validate audio frame count
            (music.stream.sampleRate > 0) &&    // Validate sample rate is supported
            (music.stream.sampleSize > 0) &&    // Validate sample size is supported
            (music.stream.channels > 0));       // Validate number of channels supported
}

int main()
{
    InitAudioDevice();
    InitWindow(WIDTH, HEIGHT, "Play with sound");
    SetTargetFPS(FPS);
    SetExitKey(KEY_ESCAPE);

    bool isLoaded = false;

    /* Music music = LoadMusicStream("resources/journey-dont-stop-believin-1981.mp3"); */
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

    /* if (executeCommand(filePath, outputFile) == 0) { */
    /*     readMetadataFile(outputFile); */
    /* } */
    /* AttachAudioStreamProcessor(music.stream, callback); */

    // Initialize FFT arrays
    for (int i = 0; i < FFT_SIZE; i++) {
        fft_in[i] = 0.0f;
        fft_out[i] = 0.0f;
    }
    for (int i = 0; i < FFT_SIZE/2; i++) {
        fft_magnitudes[i] = 0.0f;
    }

    int filePathCounter = 0;
    char *filePaths[MAX_FILEPATH_RECORDED] = { 0 }; 
    for (int i = 0; i < MAX_FILEPATH_RECORDED; i++) {
        filePaths[i] = (char *)RL_CALLOC(MAX_FILEPATH_SIZE, 1);
    }
    Music music = { 0 };

    while (!WindowShouldClose()) {
        if (IsFileDropped()) {
            FilePathList droppedFiles = LoadDroppedFiles();
            if (droppedFiles.count > 0) {
                const char *file_path = droppedFiles.paths[0];
                if (isAudioFile(file_path)) {
                    printf("File dropped: %s\n", file_path);
                    strncpy(filePaths[filePathCounter], file_path, 511);
                    filePaths[filePathCounter][511] = '\0';  // Ensure null-terminated
                    filePathCounter++;

                    if (IsMusicReady(music)) {
                        StopMusicStream(music);
                        UnloadMusicStream(music);
                    }
                    if (executeCommand(file_path, outputFile) == 0) {
                        readMetadataFile(outputFile);
                    }
                    music = LoadMusicStream(file_path);
                    AttachAudioStreamProcessor(music.stream, callback);
                } else {
                    DrawText("The extension of the file either non-support or not an mp3 file", 100, 100, 20, RED);
                }
            }
            UnloadDroppedFiles(droppedFiles);
        } 

        float delta_time = GetFrameTime();
        global_time += delta_time;

        BeginDrawing();
        ClearBackground(BLACK);
        if (filePathCounter == 0) {
            int widthText = MeasureText("Drop your files to this window!", 60);
            DrawText("Drop your files to this window!", WIDTH - widthText - 60, HEIGHT/2, 60, DARKGRAY);
        }
        else {
            DrawText("Drop new files...", 100, 110 + 40*filePathCounter, 20, DARKGRAY);
        }

        DrawFPS(10, 10);

        if (!IsMusicValid(music)) {
            DrawText("Cannot playing music", 100, 100, 20, RED);
        } else {

            if (filePathCounter > 0) {
                UpdateMusicStream(music);
                getInforGuide(&music);
                drawProgressBar(&music);
                toggleMusic(&music);
                handleProgressBarClick(&music, GetMousePosition());
                drawMetadataText(font, lines, g_lineCount);
                handleSeekMusic(&music);
                drawWaveform(g_frames, g_frames_count);
            }

        }
        EndDrawing();
    }

    for (int i = 0; i < MAX_FILEPATH_RECORDED; i++)
    {
        RL_FREE(filePaths[i]); // Free allocated memory for all filepaths
    }
    UnloadFont(font);
    UnloadMusicStream(music);
    CloseAudioDevice();
    CloseWindow();
    return 0;
}
