#define _USE_MATH_DEFINES
#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>

#define W 1000
#define H 600
#define G_EARTH 800.0
#define MAX_PARTICLES 150
#define MAX_RAIN 500
#define MAX_ACTIVE_ARROWS 5

struct Vec2 { double x, y; };
struct Arrow { Vec2 pos, vel; int active; };
struct Button { SDL_Rect rect; int state; const char* label; };
struct Particle { double x, y, speed; int size; };
struct RainDrop { double x, y, speed, length; int depth; };

struct StuckArrow { double offsetY; double angle; };
struct GroundArrow { double x, y, angle; };

Arrow arrows[MAX_ACTIVE_ARROWS] = {0};
Vec2 playerPos = {150, 450};
Vec2 targetPos = {800, 400};
int score = 0;
int arrowsLeft = 3;
int gameOver = 0;
int isPaused = 0;
int timeScale = 100;

StuckArrow stuckArrows[1000];
int numStuck = 0;
GroundArrow groundArrows[1000];
int numGroundArrows = 0;

Button btnWind    = {{10, 10, 110, 30}, 0, "GIO: OFF"};
Button btnWindDec = {{125, 10, 30, 30}, 0, "<-"};
Button btnWindInc = {{160, 10, 30, 30}, 0, "->"};
Button btnMove    = {{10, 45, 110, 30}, 0, "MOVE: OFF"};
Button btnMoveDec = {{125, 45, 30, 30}, 0, "<-"};
Button btnMoveInc = {{160, 45, 30, 30}, 0, "->"};
Button btnEnv     = {{10, 80, 110, 30}, 0, "MT: EARTH"};
Button btnDrag    = {{10, 115, 110, 30}, 1, "LUC CAN: ON"};
Button btnTraceToggle = {{10, 150, 110, 30}, 1, "TRACER: ON"};
Button btnWeather     = {{10, 185, 110, 30}, 0, "WEATHER: OFF"};
Button btnDistDec     = {{W - 250, 115, 30, 30}, 0, "<-"};
Button btnDist        = {{W - 215, 115, 120, 30}, 0, "KC: 800"};
Button btnDistInc     = {{W - 90, 115, 30, 30}, 0, "->"};
Button btnReset       = {{W - 250, 150, 190, 30}, 0, "  RESET MO PHONG"};
Button btnPause       = {{W - 250, 185, 190, 30}, 0, "DUNG THOI GIAN (E)"};

Button btnTimeDec     = {{W - 250, 220, 30, 30}, 0, "<-"};
Button btnTime        = {{W - 215, 220, 120, 30}, 0, "TOC DO: 100%"};
Button btnTimeInc     = {{W - 90, 220, 30, 30}, 0, "->"};

int targetSpeedPercent = 100;
double targetPhase = 0.729727;
double windForce = 0;
Particle clouds[MAX_PARTICLES];
RainDrop rain[MAX_RAIN];
int envMode = 0;
int traceEnv = 1;
double maxVelocity = 0;
double impactVelocity = 0;

double inputAngle = 45.0;
double inputForce = 400.0;
double currentAngle = 0, curForce = 0;
int isWeatherOn = 0;

double cZ = 1.0, offX = 0, offY = 0;
#define TX(x) ((int)((x) * cZ + offX))
#define TY(y) ((int)((y) * cZ + offY))
#define TS(s) ((int)((s) * cZ))

int isClicked(SDL_Rect r, int mx, int my) {
    return mx >= r.x && mx <= r.x + r.w && my >= r.y && my <= r.y + r.h;
}

void drawCircle(SDL_Renderer* ren, int cx, int cy, int radius) {
    for (int dy = -radius; dy <= radius; dy++) {
        int dx = (int)sqrt(radius * radius - dy * dy);
        SDL_RenderDrawLine(ren, cx - dx, cy + dy, cx + dx, cy + dy);
    }
}

void renderTextOutline(SDL_Renderer* ren, TTF_Font* font, const char* text, int x, int y, SDL_Color color) {
    if (!font) return;
    SDL_Color outlineColor = {0, 0, 0, 255};
    SDL_Surface* bg = TTF_RenderText_Blended(font, text, outlineColor);
    SDL_Texture* tBg = SDL_CreateTextureFromSurface(ren, bg);
    SDL_Rect rBg = {x + 1, y + 1, bg->w, bg->h};
    SDL_RenderCopy(ren, tBg, NULL, &rBg);
    SDL_FreeSurface(bg); SDL_DestroyTexture(tBg);

    SDL_Surface* fg = TTF_RenderText_Blended(font, text, color);
    SDL_Texture* tFg = SDL_CreateTextureFromSurface(ren, fg);
    SDL_Rect rFg = {x, y, fg->w, fg->h};
    SDL_RenderCopy(ren, tFg, NULL, &rFg);
    SDL_FreeSurface(fg); SDL_DestroyTexture(tFg);
}

void drawEarth(SDL_Renderer* ren, int cx, int cy, int r) {
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    for(int i = 8; i >= 0; i--) {
        SDL_SetRenderDrawColor(ren, 100, 200, 255, 15 + (8-i)*3);
        drawCircle(ren, cx, cy, r + i);
    }
    for(int dy = -r; dy <= r; dy++) {
        int dx = (int)sqrt(r*r - dy*dy);
        for(int x = -dx; x <= dx; x++) {
            double lat = (double)dy / r; double lon = (double)x / r;
            double n_land = sin(lat*4.5) * cos(lon*4.5) + sin(lon*2.0 + lat*3.0);
            double n_cloud = cos(lat*6.0) * sin(lon*3.0) + sin(lon*5.0 - lat*2.0);
            
            int rr, gg, bb;
            if (n_cloud > 0.6) { rr=240; gg=240; bb=255; }
            else if (n_land > 0.2) { rr=34; gg=139; bb=34; }
            else { rr=20; gg=80; bb=180; }
            
            double light = 1.0;
            if (x > dx*0.1 - dy*0.3) light = 0.15;
            else if (x > dx*-0.1 - dy*0.3) light = 0.5;
            
            SDL_SetRenderDrawColor(ren, (int)(rr*light), (int)(gg*light), (int)(bb*light), 255);
            SDL_RenderDrawPoint(ren, cx + x, cy + dy);
        }
    }
}

void drawBow(SDL_Renderer* ren, double cx, double cy, double angle, double pull_dist) {
    double bow_radius = 50.0 * cZ;
    for(double a = -1.2; a <= 1.2; a += 0.01) {
        int thickness = (int)(4.0 * cos(a * 1.2) * cZ);
        if(thickness < 1) thickness = 1;
        SDL_SetRenderDrawColor(ren, 110 - fabs(a)*20, 50 - fabs(a)*10, 10, 255);
        for(int w = -thickness; w <= thickness; w++) {
            double px = cx + cos(angle + a) * (bow_radius + w) - cos(angle)*10*cZ;
            double py = cy + sin(angle + a) * (bow_radius + w) - sin(angle)*10*cZ;
            SDL_RenderDrawPoint(ren, (int)px, (int)py);
        }
        SDL_SetRenderDrawColor(ren, 160 - fabs(a)*30, 80 - fabs(a)*20, 20, 255);
        double px = cx + cos(angle + a) * bow_radius - cos(angle)*10*cZ; double py = cy + sin(angle + a) * bow_radius - sin(angle)*10*cZ;
        SDL_RenderDrawPoint(ren, (int)px, (int)py);
    }
    
    SDL_SetRenderDrawColor(ren, 20, 20, 20, 255);
    for(double a = -0.2; a <= 0.2; a += 0.01) {
        for(int w = (int)(-5.0*cZ); w <= (int)(5.0*cZ); w++) {
            if ((int)(a*100) % 6 == 0) continue;
            double px = cx + cos(angle + a) * (bow_radius + w) - cos(angle)*10*cZ; double py = cy + sin(angle + a) * (bow_radius + w) - sin(angle)*10*cZ;
            SDL_RenderDrawPoint(ren, (int)px, (int)py);
        }
    }
    
    SDL_SetRenderDrawColor(ren, 255, 200, 50, 255);
    double tX = cx + cos(angle - 1.2) * bow_radius - cos(angle)*10*cZ; double tY = cy + sin(angle - 1.2) * bow_radius - sin(angle)*10*cZ;
    double bX = cx + cos(angle + 1.2) * bow_radius - cos(angle)*10*cZ; double bY = cy + sin(angle + 1.2) * bow_radius - sin(angle)*10*cZ;
    int tipS = static_cast<int>(fmax(3, TS(6)));
    SDL_Rect topTip = {(int)tX-tipS/2, (int)tY-tipS/2, tipS, tipS}; SDL_RenderFillRect(ren, &topTip);
    SDL_Rect botTip = {(int)bX-tipS/2, (int)bY-tipS/2, tipS, tipS}; SDL_RenderFillRect(ren, &botTip);
    
    SDL_SetRenderDrawColor(ren, 240, 240, 240, 255);
    double pullX, pullY;
    if (pull_dist > 0) { pullX = cx - cos(angle) * pull_dist * cZ; pullY = cy - sin(angle) * pull_dist * cZ; }
    else { pullX = (tX + bX) / 2.0; pullY = (tY + bY) / 2.0; }
    SDL_RenderDrawLine(ren, (int)tX, (int)tY, (int)pullX, (int)pullY); SDL_RenderDrawLine(ren, (int)bX, (int)bY, (int)pullX, (int)pullY);
}

void drawArrow(SDL_Renderer* ren, double cx, double cy, double angle) {
    double L = 50.0 * cZ;
    double tipX = cx + cos(angle) * L/2; double tipY = cy + sin(angle) * L/2;
    double tailX = cx - cos(angle) * L/2; double tailY = cy - sin(angle) * L/2;
    
    SDL_SetRenderDrawColor(ren, 120, 70, 30, 255);
    for(int w=-1; w<=1; w++) SDL_RenderDrawLine(ren, (int)tailX, (int)tailY+w, (int)tipX, (int)tipY+w);
    
    SDL_SetRenderDrawColor(ren, 220, 220, 230, 255);
    double headL = 12.0 * cZ;
    double a1 = angle + M_PI * 0.85; double a2 = angle - M_PI * 0.85;
    for(int i=-2; i<=2; i++) {
        SDL_RenderDrawLine(ren, (int)tipX, (int)tipY, (int)(tipX + cos(a1)*headL) + i, (int)(tipY + sin(a1)*headL));
        SDL_RenderDrawLine(ren, (int)tipX, (int)tipY, (int)(tipX + cos(a2)*headL) + i, (int)(tipY + sin(a2)*headL));
    }
    
    for(int i=0; i<5; i++) {
        double fx = cx - cos(angle) * (L/2 - i*2.5*cZ); double fy = cy - sin(angle) * (L/2 - i*2.5*cZ);
        SDL_SetRenderDrawColor(ren, (i%2==0)?255:220, (i%2==0)?40:220, (i%2==0)?40:220, 255);
        for(int w=0; w<=1; w++) {
            SDL_RenderDrawLine(ren, (int)fx, (int)fy, (int)(fx + cos(angle+M_PI*0.75)*6*cZ) + w, (int)(fy + sin(angle+M_PI*0.75)*6*cZ));
            SDL_RenderDrawLine(ren, (int)fx, (int)fy, (int)(fx + cos(angle-M_PI*0.75)*6*cZ) + w, (int)(fy + sin(angle-M_PI*0.75)*6*cZ));
        }
    }
}

int main(int argc, char* argv[]) {
    srand((unsigned int)time(NULL));
    SDL_Init(SDL_INIT_VIDEO);
    if (TTF_Init() == -1) return -1;
    
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    
    TTF_Font* font = TTF_OpenFont("arial.ttf", 16);
    TTF_Font* fontLarge = TTF_OpenFont("arial.ttf", 24);
    TTF_Font* fontFormula = TTF_OpenFont("arial.ttf", 14);

    SDL_Window* win = SDL_CreateWindow("MO PHONG CHUYEN DONG MUI TEN", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, W, H, SDL_WINDOW_FULLSCREEN_DESKTOP);
    SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    
    SDL_RenderSetLogicalSize(ren, W, H);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);

    for(int i=0; i<MAX_PARTICLES; i++) clouds[i] = Particle{(double)(rand()%10000), (double)(rand()%H), (rand()%50+50)/100.0, rand()%3+2};
    for(int i=0; i<MAX_RAIN; i++) rain[i] = RainDrop{(double)(rand()%(W+400)-200), (double)(rand()%H), 15.0 + rand()%15, 10.0 + rand()%20, rand()%3 + 1};

    int running = 1, dragging = 0;
    Vec2 dragStart;
    double curVX = 0, curVY = 0;
    SDL_Color colorWhite = {255, 255, 255, 255};
    char windLabel[32], moveLabel[32], distLabel[32], timeLabel[32];

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = 0;
            
            if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_ESCAPE) running = 0;
                if (e.key.keysym.sym == SDLK_e) { isPaused = !isPaused; btnPause.state = isPaused; }
                
                if (e.key.keysym.sym == SDLK_UP) inputAngle = (inputAngle < 90) ? inputAngle + 1 : 90;
                if (e.key.keysym.sym == SDLK_DOWN) inputAngle = (inputAngle > -90) ? inputAngle - 1 : -90;

                if (e.key.keysym.sym == SDLK_z) inputForce -= 1;
                if (e.key.keysym.sym == SDLK_x) inputForce += 1;
                if (e.key.keysym.sym == SDLK_LEFT) inputForce -= 10;
                if (e.key.keysym.sym == SDLK_RIGHT) inputForce += 10;
                if (e.key.keysym.sym == SDLK_c) inputForce -= 100;
                if (e.key.keysym.sym == SDLK_v) inputForce += 100;
                if (e.key.keysym.sym == SDLK_b) inputForce -= 1000;
                if (e.key.keysym.sym == SDLK_n) inputForce += 1000;
                if (inputForce < 0) inputForce = 0;
                
                if (e.key.keysym.sym == SDLK_a) { windForce -= 20; btnWind.state = (windForce != 0); }
                if (e.key.keysym.sym == SDLK_d) { windForce += 20; btnWind.state = (windForce != 0); }
                
                if (e.key.keysym.sym == SDLK_f) { traceEnv = !traceEnv; btnTraceToggle.state = traceEnv; }
                if (e.key.keysym.sym == SDLK_r) { score = 0; numStuck = 0; numGroundArrows = 0;
                    for(int i = 0; i < MAX_ACTIVE_ARROWS; i++) arrows[i].active = 0;
                    maxVelocity = 0; impactVelocity = 0;
                }

                int activeCount = 0;
                for(int i = 0; i < MAX_ACTIVE_ARROWS; i++) if(arrows[i].active) activeCount++;
                
                if (e.key.keysym.sym == SDLK_SPACE && arrowsLeft > 0 && !gameOver && activeCount == 0 && !isPaused) {
                    score = 0;
                    currentAngle = inputAngle; curForce = inputForce;
                    double rad = inputAngle * M_PI / 180.0;
                    curVX = inputForce * cos(rad); curVY = -inputForce * sin(rad);
                    
                    for(int i = 0; i < MAX_ACTIVE_ARROWS; i++) {
                        if(!arrows[i].active) {
                            arrows[i].pos = playerPos;
                            arrows[i].vel = Vec2{curVX, curVY};
                            arrows[i].active = 1;
                            maxVelocity = 0; impactVelocity = 0;
                            break;
                        }
                    }
                }
            }
            
            if (e.type == SDL_MOUSEBUTTONDOWN) {
                int mx = e.button.x; int my = e.button.y; 
                
                if (isClicked(btnReset.rect, mx, my)) {
                    score = 0; numStuck = 0; numGroundArrows = 0;
                    for(int i = 0; i < MAX_ACTIVE_ARROWS; i++) arrows[i].active = 0;
                    maxVelocity = 0; impactVelocity = 0;
                } else if (isClicked(btnPause.rect, mx, my)) {
                    isPaused = !isPaused; btnPause.state = isPaused;
                } else if (isClicked(btnTimeDec.rect, mx, my)) {
                    if (timeScale > 0) timeScale -= 5;
                } else if (isClicked(btnTimeInc.rect, mx, my)) {
                    if (timeScale < 100) timeScale += 5;
                } else {
                    if (isClicked(btnWind.rect, mx, my)) { btnWind.state = !btnWind.state; windForce = btnWind.state ? 100 : 0; }
                    else if (isClicked(btnWindDec.rect, mx, my)) { windForce -= 20; btnWind.state = (windForce != 0); }
                    else if (isClicked(btnWindInc.rect, mx, my)) { windForce += 20; btnWind.state = (windForce != 0); }
                    else if (isClicked(btnMove.rect, mx, my)) { btnMove.state = !btnMove.state; if(btnMove.state && targetSpeedPercent == 0) targetSpeedPercent = 100; }
                    else if (isClicked(btnMoveDec.rect, mx, my)) { if (targetSpeedPercent > 0) targetSpeedPercent -= 5; if (targetSpeedPercent == 0) btnMove.state = 0; }
                    else if (isClicked(btnMoveInc.rect, mx, my)) { if (targetSpeedPercent < 200) targetSpeedPercent += 5; if (targetSpeedPercent > 0) btnMove.state = 1; }
                    else if (isClicked(btnDrag.rect, mx, my)) { btnDrag.state = !btnDrag.state; btnDrag.label = btnDrag.state ? "LUC CAN: ON" : "LUC CAN: OFF"; }
                    else if (isClicked(btnEnv.rect, mx, my)) { envMode = (envMode + 1) % 4; windForce = 0; btnWind.state = 0; btnEnv.label = envMode == 0 ? "ENV: EARTH" : envMode == 1 ? "ENV: WATER" : envMode == 2 ? "ENV: MOON" : "ENV: SPACE"; }
                    else if (isClicked(btnTraceToggle.rect, mx, my)) { btnTraceToggle.state = !btnTraceToggle.state; traceEnv = btnTraceToggle.state; }
                    else if (isClicked(btnWeather.rect, mx, my)) { isWeatherOn = !isWeatherOn; btnWeather.state = isWeatherOn; }
                    else if (isClicked(btnDistDec.rect, mx, my)) { targetPos.x -= 100; if(targetPos.x < 300) targetPos.x = 300; }
                    else if (isClicked(btnDistInc.rect, mx, my)) { targetPos.x += 100; if(targetPos.x > 50000) targetPos.x = 50000; }
                    else { dragging = 1; dragStart = Vec2{(double)mx, (double)my}; }
                }
            }
            
            if (e.type == SDL_MOUSEBUTTONUP && dragging) {
                dragging = 0;
                score = 0;
                int activeCount = 0;
                for(int i = 0; i < MAX_ACTIVE_ARROWS; i++) if(arrows[i].active) activeCount++;
                if (arrowsLeft > 0 && !gameOver && activeCount == 0 && !isPaused) {
                    for(int i = 0; i < MAX_ACTIVE_ARROWS; i++) {
                        if(!arrows[i].active) {
                            arrows[i].pos = playerPos; arrows[i].vel = Vec2{curVX, curVY};
                            arrows[i].active = 1;
                            maxVelocity = 0; impactVelocity = 0;
                            break;
                        }
                    }
                }
            }
            
            if (e.type == SDL_MOUSEMOTION && dragging) {
                int mx = e.motion.x; int my = e.motion.y;
                curVX = (dragStart.x - mx) * 3; curVY = (dragStart.y - my) * 3;
                currentAngle = atan2(-curVY, curVX) * 180.0 / M_PI; curForce = sqrt(curVX*curVX + curVY*curVY);
                inputAngle = currentAngle; inputForce = curForce;
            }
        }

        cZ = 1.0;
        if (targetPos.x > 800) cZ = 650.0 / (targetPos.x - 150.0); 
        offX = 150.0 * (1.0 - cZ); 
        offY = 450.0 * (1.0 - cZ); 
        
        double camLeft = -offX / cZ; double camRight = (W - offX) / cZ;

        if (btnWind.state) sprintf(windLabel, "GIO: %.0f", windForce); else sprintf(windLabel, "GIO: OFF"); btnWind.label = windLabel;
        if (btnMove.state && targetSpeedPercent > 0) sprintf(moveLabel, "MOVE: %d%%", targetSpeedPercent); else sprintf(moveLabel, "MOVE: OFF"); btnMove.label = moveLabel;
        sprintf(distLabel, "KC %.0f m", targetPos.x); btnDist.label = distLabel;
        sprintf(timeLabel, "TOC DO: %d%%", timeScale); btnTime.label = timeLabel;
        btnTraceToggle.label = traceEnv ? "TRACER: ON" : "TRACER: OFF";
        btnWeather.label = isWeatherOn ? "WEATHER: ON" : "WEATHER: OFF";

        double currentG = G_EARTH;
        double base_friction = btnDrag.state ? 0.0005 : 0.0;
        double rainPushForce = 0.0;
        
        if (envMode == 0 && isWeatherOn && btnDrag.state) {
            base_friction += 0.0015;
            rainPushForce = 350.0;
        }
        
        double friction = base_friction;
        if (envMode == 1) { currentG = G_EARTH * 0.3; friction = btnDrag.state ? base_friction * 900.0 : 0.0; }
        else if (envMode == 2) { currentG = G_EARTH * 1/6.0; friction = 0.0; } 
        else if (envMode == 3) { currentG = 0.0; friction = 0.0; }

        double dt = 0.016 * (timeScale / 100.0);

        if (!isPaused && timeScale > 0) {
            if (btnMove.state && targetSpeedPercent > 0) {
                targetPhase += (targetSpeedPercent / 100.0) * 0.002 * (16.0 * (timeScale / 100.0)); targetPos.y = 300 + sin(targetPhase) * 150;
            }

            double scale = timeScale / 100.0;
            double dt_calc = 0.016 * scale;
            for(int k = 0; k < MAX_ACTIVE_ARROWS; k++) {
            if (arrows[k].active) {
    double prevX = arrows[k].pos.x; double prevY = arrows[k].pos.y;
    
    double oldVx = arrows[k].vel.x;
    double oldVy = arrows[k].vel.y;

    arrows[k].vel.x -= arrows[k].vel.x * friction * scale;
    arrows[k].vel.y -= arrows[k].vel.y * friction * scale;
    arrows[k].vel.y += (currentG + rainPushForce) * dt_calc;
    if(envMode == 0) arrows[k].vel.x += windForce * 0.05 * scale;

    double currentSpeed = sqrt(arrows[k].vel.x*arrows[k].vel.x + arrows[k].vel.y*arrows[k].vel.y);
    if (currentSpeed > maxVelocity) maxVelocity = currentSpeed;

    arrows[k].pos.x += (oldVx + arrows[k].vel.x) * 0.5 * dt_calc;
    arrows[k].pos.y += (oldVy + arrows[k].vel.y) * 0.5 * dt_calc;

                    double nextX = arrows[k].pos.x; double nextY = arrows[k].pos.y;
                    double ang_col = atan2(arrows[k].vel.y, arrows[k].vel.x);
                    
                    double faceX = (arrows[k].vel.x >= 0) ? (targetPos.x - 7.0) : (targetPos.x + 7.0);

                    if ((prevX < faceX && nextX >= faceX) || (prevX > faceX && nextX <= faceX)) {
                        double denom = nextX - prevX;
                        if (fabs(denom) > 0.001) {
                            double factor = (faceX - prevX) / denom;
                            double y_col = prevY + factor * (nextY - prevY);
                            double dy = fabs(y_col - targetPos.y);

                            if (dy <= 75) { 
                                if (dy <= 5) score += 10;
                                else if (dy <= 20) score += 8;
                                else if (dy <= 35) score += 6;
                                else if (dy <= 55) score += 4;
                                else score += 2;

                                impactVelocity = currentSpeed;
                                arrows[k].active = 0;
                                
                                if(numStuck < 1000) {
                                    stuckArrows[numStuck].offsetY = y_col - targetPos.y;
                                    stuckArrows[numStuck].angle = ang_col;
                                    numStuck++;
                                }
                            }
                        }
                    }

                    double groundY = H + 1000;
                    if (envMode == 0) groundY = H - 100;
                    else if (envMode == 2) {
                        double dx = arrows[k].pos.x - W/2;
                        if (fabs(dx) < 1300) groundY = (H + 1200) - sqrt(1300*1300 - dx*dx);
                    }

                    if (arrows[k].active && arrows[k].pos.y >= groundY) {
                        if (numGroundArrows < 1000) {
                            groundArrows[numGroundArrows].x = arrows[k].pos.x;
                            groundArrows[numGroundArrows].y = groundY;
                            groundArrows[numGroundArrows].angle = ang_col;
                            numGroundArrows++;
                        }
                        arrows[k].active = 0;
                    }
                    if (arrows[k].pos.y > targetPos.y + 3000 || arrows[k].pos.x > targetPos.x + 3000 || arrows[k].pos.x < -100 || arrows[k].pos.y < -1000) arrows[k].active = 0;
                }
            }
        }

        if (envMode == 0) { 
            for (int y = 0; y < H; y++) {
                SDL_SetRenderDrawColor(ren, 135 - y*40/H, 206 - y*40/H, 235, 255);
                SDL_RenderDrawLine(ren, 0, y, W, y);
            }
            SDL_SetRenderDrawColor(ren, 255, 235, 100, 255); drawCircle(ren, W - 120, 100, 45); 

            double m_width = 450.0;
            int mStart = (int)floor(camLeft / m_width); int mEnd = (int)floor(camRight / m_width) + 1;
            for(int m = mStart; m <= mEnd; m++) {
                int peakX = TX(m * m_width + 150); int peakY = TY(H - 350 + (abs(m)%2)*40); int mH = TS(250);
                for (int dy = 0; dy < mH; dy++) {
                    if (dy < mH * 0.2) SDL_SetRenderDrawColor(ren, 245, 245, 255, 255); 
                    else SDL_SetRenderDrawColor(ren, 100 - dy*20/mH, 130 - dy*30/mH, 100 - dy*20/mH, 255);
                    SDL_RenderDrawLine(ren, peakX - (int)(dy * 1.5), peakY + dy, peakX + (int)(dy * 1.5), peakY + dy);
                }
            }
            SDL_SetRenderDrawColor(ren, 64, 164, 223, 255); SDL_Rect lake = {0, TY(H - 180), W, H}; SDL_RenderFillRect(ren, &lake);
            SDL_SetRenderDrawColor(ren, 34, 139, 34, 255); SDL_Rect grass = {0, TY(H - 100), W, H}; SDL_RenderFillRect(ren, &grass);

            double g_width = 40.0;
            int gStart = (int)floor(camLeft / g_width); int gEnd = (int)floor(camRight / g_width) + 1;
            SDL_SetRenderDrawColor(ren, 0, 100, 0, 255);
            for(int c = gStart; c <= gEnd; c++) {
                int bx = TX(c * g_width + 20); int y1 = TY(H - 100); int y2 = TY(H - 115 + (abs(c)%4)*5); int y3 = TY(H - 105);
                SDL_RenderDrawLine(ren, bx, y1, bx, y2); SDL_RenderDrawLine(ren, bx, y2, bx - TS(5), y3); SDL_RenderDrawLine(ren, bx, y2, bx + TS(5), y3);
            }
            double t_width = 600.0;
            int tStart = (int)floor(camLeft / t_width); int tEnd = (int)floor(camRight / t_width) + 1;
            for(int t = tStart; t <= tEnd; t++) {
                int tx = TX(t * t_width + 80);
                SDL_SetRenderDrawColor(ren, 139, 69, 19, 255); SDL_Rect t1 = {tx, TY(H - 150), static_cast<int>(fmax(1,TS(15))), TS(60)}; SDL_RenderFillRect(ren, &t1);
                SDL_SetRenderDrawColor(ren, 0, 100, 0, 255); drawCircle(ren, tx + TS(7), TY(H - 150), TS(35)); drawCircle(ren, tx - TS(13), TY(H - 130), TS(25)); drawCircle(ren, tx + TS(27), TY(H - 130), TS(25));
            }

            SDL_SetRenderDrawColor(ren, 255, 255, 255, 200);
            for(int i=0; i<MAX_PARTICLES; i++) {
                if (!isPaused && timeScale > 0) clouds[i].x += (20.0 + (btnWind.state ? windForce : 0)) * clouds[i].speed * (0.02 * (timeScale / 100.0));
                if (clouds[i].x > 20000) clouds[i].x = 0; if (clouds[i].x < -10000) clouds[i].x = 10000;
                double screenX = fmod(clouds[i].x - camLeft * 0.1 * clouds[i].speed, W); if (screenX < 0) screenX += W;
                SDL_Rect r = {(int)screenX, (int)clouds[i].y % 250, clouds[i].size * 8, clouds[i].size * 3}; SDL_RenderFillRect(ren, &r);
            }

            if (isWeatherOn) {
                for (int i = 0; i < MAX_RAIN; i++) {
                    if (!isPaused && timeScale > 0) {
                        rain[i].y += rain[i].speed * (timeScale / 100.0); 
                        rain[i].x += windForce * 0.05 * rain[i].depth * 0.5 * (timeScale / 100.0);
                        if (rain[i].y > H) { rain[i].y = -20; rain[i].x = (double)(rand()%(W+400)-200); }
                        if (rain[i].x > W + 200) rain[i].x -= (W + 400); if (rain[i].x < -200) rain[i].x += (W + 400);
                    }
                    int alpha = 100 + rain[i].depth * 50; SDL_SetRenderDrawColor(ren, 180, 200, 255, alpha);
                    SDL_RenderDrawLine(ren, (int)rain[i].x, (int)rain[i].y, (int)(rain[i].x + windForce * 0.02 * rain[i].depth * 0.5), (int)(rain[i].y + rain[i].length));
                }
            }
        } else if (envMode == 1) { 
            for (int y = 0; y < H; y++) { SDL_SetRenderDrawColor(ren, 0, 50 + y*50/H, 100 + y*100/H, 255); SDL_RenderDrawLine(ren, 0, y, W, y); }
            SDL_SetRenderDrawColor(ren, 150, 200, 255, 150);
            for(int i=0; i<MAX_PARTICLES; i++) {
                if (!isPaused && timeScale > 0) { clouds[i].y -= clouds[i].speed * 1.5 * (timeScale / 100.0); if(clouds[i].y < 0) clouds[i].y = H; }
                double screenX = fmod(clouds[i].x - camLeft * 0.2, W); if (screenX < 0) screenX += W;
                SDL_Rect r = {(int)screenX, (int)clouds[i].y, clouds[i].size, clouds[i].size}; SDL_RenderDrawRect(ren, &r);
            }

        } else if (envMode == 2) {
            SDL_SetRenderDrawColor(ren, 5, 5, 10, 255); SDL_RenderClear(ren); SDL_SetRenderDrawColor(ren, 255, 255, 255, 200);
            for(int i=0; i<MAX_PARTICLES; i++) {
                double screenX = fmod(clouds[i].x - camLeft * 0.02, W); if (screenX < 0) screenX += W;
                SDL_Rect r = {(int)screenX, (int)clouds[i].y, static_cast<int>(fmax(1,TS(2))), static_cast<int>(fmax(1,TS(2)))}; SDL_RenderFillRect(ren, &r);
            }
            drawEarth(ren, W/2, 220, 65);
            
            double crater_w = 400.0; int crStart = (int)floor(camLeft / crater_w); int crEnd = (int)floor(camRight / crater_w) + 1;
            SDL_SetRenderDrawColor(ren, 150, 150, 150, 255);
            for(int cr = crStart; cr <= crEnd; cr++) drawCircle(ren, TX(cr * crater_w + 150), TY(H + 1200), TS(1300));
        } else if (envMode == 3) {
            SDL_SetRenderDrawColor(ren, 2, 2, 8, 255); SDL_RenderClear(ren);
            
            SDL_SetRenderDrawColor(ren, 255, 240, 200, 30); drawCircle(ren, W/2, H/2, 90); SDL_SetRenderDrawColor(ren, 255, 250, 220, 60); drawCircle(ren, W/2, H/2, 50); SDL_SetRenderDrawColor(ren, 255, 255, 255, 150); drawCircle(ren, W/2, H/2, 15);
            for (int i = 0; i < 2000; i++) {
                double t = i * 0.05; double r = t * 18.0;
                for(int j = 0; j < 2; j++) {
                    double angle = t + (j * M_PI) + (sin(i*113.0) * 0.5); int px = W/2 + cos(angle) * r * 1.8; int py = H/2 + sin(angle) * r * 0.7;
                    if (px >= 0 && px < W && py >= 0 && py < H) { SDL_SetRenderDrawColor(ren, 80 + (i%100), 100 + (i%155), 255, 120); SDL_RenderDrawPoint(ren, px, py); }
                }
            }
            SDL_SetRenderDrawColor(ren, 255, 255, 255, 180);
            for(int i=0; i<MAX_PARTICLES; i++) { double screenX = fmod(clouds[i].x - camLeft * 0.05, W); if (screenX < 0) screenX += W; SDL_Rect r = {(int)screenX, (int)clouds[i].y, 2, 2}; SDL_RenderFillRect(ren, &r); }
        }

        SDL_SetRenderDrawColor(ren, 218, 165, 32, 255);
        SDL_Rect rim = {static_cast<int>(TX(targetPos.x) - fmax(1,TS(10))), TY(targetPos.y - 85), static_cast<int>(fmax(2,TS(20))), static_cast<int>(fmax(2,TS(170)))};
        SDL_RenderFillRect(ren, &rim);
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 255); SDL_RenderDrawRect(ren, &rim);

        int targetColors[5][3] = { {255, 255, 255}, {30, 30, 30}, {0, 150, 255}, {255, 50, 50}, {255, 220, 0} };
        int targetHeights[5] = {150, 110, 70, 40, 10};
        for (int i = 0; i < 5; i++) {
            SDL_SetRenderDrawColor(ren, targetColors[i][0], targetColors[i][1], targetColors[i][2], 255);
            SDL_Rect tr = {static_cast<int>(TX(targetPos.x) - fmax(1,TS(7))), TY(targetPos.y - targetHeights[i]/2), static_cast<int>(fmax(2,TS(14))), static_cast<int>(fmax(2,TS(targetHeights[i])))};
            SDL_RenderFillRect(ren, &tr);
            SDL_SetRenderDrawColor(ren, 0, 0, 0, 100); SDL_RenderDrawRect(ren, &tr); 
        }

        if (traceEnv) {
            double simX = playerPos.x, simY = playerPos.y;
            double rad_sim = (dragging ? currentAngle : inputAngle) * M_PI / 180.0;
            double simVX = (dragging ? curForce : inputForce) * cos(rad_sim);
            double simVY = -(dragging ? curForce : inputForce) * sin(rad_sim);
            double simG_trace = currentG, simFriction_trace = friction;
            double simWind_trace = (envMode == 0) ? windForce : 0;

    SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
        for (int i = 0; i < 25; i++) {
            double prevX = simX, prevY = simY;
        for (int step = 0; step < 4; step++) {
            double oldSimVx = simVX;
            double oldSimVy = simVY;

            simVX -= simVX * simFriction_trace; 
            simVY -= simVY * simFriction_trace;
            simVY += (simG_trace + rainPushForce) * 0.016; 
            simVX += simWind_trace * 0.05;

        simX += (oldSimVx + simVX) * 0.5 * 0.016;
        simY += (oldSimVy + simVY) * 0.5 * 0.016;
    }
                
                double faceX = (simVX >= 0) ? (targetPos.x - 7.0) : (targetPos.x + 7.0);
                if ((prevX < faceX && simX >= faceX) || (prevX > faceX && simX <= faceX)) {
                    double fact = (faceX - prevX) / (simX - prevX);
                    double colY = prevY + fact * (simY - prevY);
                    if (fabs(colY - targetPos.y) <= 75) {
                        drawCircle(ren, TX(faceX), TY(colY), static_cast<int>(fmax(1, TS(3)))); break;
                    }
                }
                drawCircle(ren, TX(simX), TY(simY), static_cast<int>(fmax(1, TS(3))));
                if (simX > camRight || simY > H+1000 || simX < camLeft) break;
            }
        }

        for(int i=0; i < numStuck; i++) {
            double faceX = (cos(stuckArrows[i].angle) >= 0) ? (targetPos.x - 7.0) : (targetPos.x + 7.0);
            double tY = targetPos.y + stuckArrows[i].offsetY;
            drawArrow(ren, TX(faceX - cos(stuckArrows[i].angle) * 25.0), TY(tY - sin(stuckArrows[i].angle) * 25.0), stuckArrows[i].angle);
        }
        for(int i=0; i < numGroundArrows; i++) {
            drawArrow(ren, TX(groundArrows[i].x - cos(groundArrows[i].angle) * 25.0), TY(groundArrows[i].y - sin(groundArrows[i].angle) * 25.0), groundArrows[i].angle);
        }

        double rad = (dragging ? currentAngle : inputAngle) * M_PI / 180.0;
        double pull_dist = dragging ? fmin(curForce * 0.1, 50.0) : fmin(inputForce * 0.1, 50.0);
        drawBow(ren, TX(playerPos.x), TY(playerPos.y), -rad, pull_dist);

        for(int k=0; k < MAX_ACTIVE_ARROWS; k++) {
            if (arrows[k].active) drawArrow(ren, TX(arrows[k].pos.x), TY(arrows[k].pos.y), atan2(arrows[k].vel.y, arrows[k].vel.x));
        }
        
        int activeCountDisplay = 0;
        for (int i = 0; i < MAX_ACTIVE_ARROWS; i++) if (arrows[i].active) activeCountDisplay++;
        if (activeCountDisplay == 0) drawArrow(ren, TX(playerPos.x - cos(-rad) * pull_dist), TY(playerPos.y - sin(-rad) * pull_dist), -rad);

        Button* btns[] = {&btnWind, &btnWindDec, &btnWindInc, &btnMove, &btnMoveDec, &btnMoveInc, &btnEnv, &btnDrag, &btnTraceToggle, &btnWeather, &btnDist, &btnDistDec, &btnDistInc, &btnPause, &btnTimeDec, &btnTime, &btnTimeInc};
        for(int i=0; i<17; i++) {
            SDL_SetRenderDrawColor(ren, btns[i]->state ? 0 : 60, btns[i]->state ? 200 : 60, 60, 255);
            SDL_RenderFillRect(ren, &btns[i]->rect);
            renderTextOutline(ren, fontFormula, btns[i]->label, btns[i]->rect.x + 8, btns[i]->rect.y + 8, colorWhite);
        }

        char strScore[64], strVel[128];
        sprintf(strScore, "DIEM CUA MUI TEN VUA BAN: %d", score);
        sprintf(strVel, "V_max: %.1f | V_tt: %.1f", maxVelocity, impactVelocity);
        
        renderTextOutline(ren, font, strScore, W - 265, 25, colorWhite);
        renderTextOutline(ren, fontFormula, strVel, W - 265, 55, colorWhite);

        SDL_SetRenderDrawColor(ren, 200, 50, 50, 255); SDL_RenderFillRect(ren, &btnReset.rect);
        renderTextOutline(ren, fontFormula, btnReset.label, btnReset.rect.x + 40, btnReset.rect.y + 8, colorWhite);

        int fx = W/2 - 150, fy = 15;
        renderTextOutline(ren, font, "--- CONG THUC ---", fx, fy, colorWhite);
        
        char f_buf[10][128];
        
        double uiAngle = dragging ? currentAngle : inputAngle;
        double uiForce = dragging ? curForce : inputForce;
        
        double dispAngle = uiAngle;
        double dispVel = uiForce;
        double v_x_rt = dispVel * cos(dispAngle * M_PI / 180.0);
        double v_y_rt = dispVel * sin(dispAngle * M_PI / 180.0);
        int isTracking = 0;

        for(int i = 0; i < MAX_ACTIVE_ARROWS; i++) {
            if(arrows[i].active) {
                v_x_rt = arrows[i].vel.x;
                v_y_rt = -arrows[i].vel.y;
                dispVel = sqrt(v_x_rt * v_x_rt + v_y_rt * v_y_rt);
                dispAngle = atan2(v_y_rt, v_x_rt) * 180.0 / M_PI;
                isTracking = 1;
                break;
            }
        }

        if (isTracking) {
            sprintf(f_buf[0], "1. v_total hien tai = %.1f px/s", dispVel);
            sprintf(f_buf[1], "2. Goc bay hien tai = %.1f do", dispAngle);
        } else {
            sprintf(f_buf[0], "1. v_total (Du kien) = %.1f px/s", dispVel);
            sprintf(f_buf[1], "2. Goc ban (Du kien) = %.1f do", dispAngle);
        }

        sprintf(f_buf[2], "3. v_x = %.1f * cos(%.1f do) = %.1f", dispVel, dispAngle, v_x_rt);
        sprintf(f_buf[3], "4. v_y = %.1f * sin(%.1f do) = %.1f", dispVel, dispAngle, v_y_rt);
        sprintf(f_buf[4], "5. Luc can (F_Can) = %.5f", friction);
        sprintf(f_buf[5], "6. Gia toc G = %.1f", currentG);
        sprintf(f_buf[6], "7. Luc ep Mua (F_Push) = %.1f", rainPushForce);
        sprintf(f_buf[7], "8. Luc can Nuoc (F_nuoc) = 900 * F_can", rainPushForce);
        
        double dt_calc = 0.016 * (timeScale / 100.0);
        double dvy = -(currentG + rainPushForce) * dt_calc; 
        double dvx = windForce * (0.05 * (timeScale / 100.0));
        
        sprintf(f_buf[8], "9. v_y(next) = %.1f - %.1f*%.4f + (%.1f) = %.1f", v_y_rt, v_y_rt, friction, dvy, v_y_rt - v_y_rt*friction + dvy);
        sprintf(f_buf[9], "10. v_x(next) = %.1f - %.1f*%.4f + (%.1f) = %.1f", v_x_rt, v_x_rt, friction, dvx, v_x_rt - v_x_rt*friction + dvx);

        for (int z = 0; z < 10; z++) renderTextOutline(ren, fontFormula, f_buf[z], fx - 60, fy + 30 + z*20, colorWhite);

        char strInput[128], strGuide[128], strFormula[128];
        sprintf(strInput, "Goc: %.1f do | Luc: %.1f", uiAngle, uiForce);
        sprintf(strGuide, "[Z/X]: +-1 | [< / >]: +-10 | [C/V]: +-100 | [B/N]: +-1000 | [ESC] Thoat");
        sprintf(strFormula, ">> Nhan [SPACE] hoac [CHUOT] de BAN <<");

        SDL_Color textColor = (envMode == 0) ? SDL_Color{255, 255, 255, 255} : colorWhite;
        renderTextOutline(ren, fontLarge, strInput, 20, H - 90, textColor);
        renderTextOutline(ren, font, strGuide, 20, H - 55, textColor);
        renderTextOutline(ren, fontLarge, strFormula, 20, H - 35, SDL_Color{255, 100, 100, 255});
        SDL_RenderPresent(ren); SDL_Delay(16);
    }
    
    TTF_CloseFont(fontFormula);
    TTF_CloseFont(font);
    TTF_CloseFont(fontLarge);
    TTF_Quit();
    SDL_Quit();
    return 0;
}