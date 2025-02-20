#include <stdio.h>
#include <raylib.h>
#include <raymath.h>

#define WIDTH 1000
#define HEIGHT 800
#define FPS 60

#define COLOR_WHITE (Color){ 255, 255, 255, 255 }
#define COLOR_YELLOW (Color){ 253, 249, 0, 255 }

#define PROGRESS_BAR_X 100
#define PROGRESS_BAR_Y 700
#define PROGRESS_BAR_WIDTH 800
#define PROGRESS_BAR_HEIGHT 20

Color headDotColor = { 255, 255, 255, 255 };

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
        /* printf("%f ", newTime); */
    }
}

void drawTextMusicInfor(Music *music)
{
    DrawText(const char *text, int posX, int posY, int fontSize, Color color);
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

int main()
{
    InitAudioDevice();
    InitWindow(WIDTH, HEIGHT, "Play with sound");
    SetTargetFPS(FPS);

    Music music = LoadMusicStream("Dream-On.mp3");

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
        }
        EndDrawing();
    }
    UnloadMusicStream(music);
    CloseAudioDevice();
    CloseWindow();
    return 0;
}
