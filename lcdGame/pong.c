#include <msp430.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libTimer.h>
#include <lcdutils.h>
#include <lcddraw.h>
#include <shape.h>
#include <abCircle.h>
#include <p2switches.h>
//#include "button.h"
//#include "sound.h"
#define GREEN_LED BIT6
//#define RED_LED BIT0

// keeps score
char scoreText[8] = "Score: ";
int score = 0;

AbRect Rect10 = {abRectGetBounds, abRectCheck, {5,20}}; /**5x20 rectangle */
//AbRArrow rightArrow = {abRArrowGetBounds, abRArrowCheck, 30};
AbRect line = {abRectGetBounds, abRectCheck, {0.5, screenHeight/2 -10}};
AbRectOutline fieldOutline = {
  abRectOutlineGetBounds, abRectOutlineCheck,
  {screenWidth/2 - 10, screenHeight/2 - 10}
};

Layer fieldLayer = { /*playing field as a layer */
  (AbShape *) &fieldOutline,
  {screenWidth/2, screenHeight/2}, /**<center */
  {0,0}, {0,0},
  COLOR_BLUE,
  0
};
Layer divisionLayer = { /*line dividing screen */
  (AbShape *) &line,
  {screenWidth/2, screenHeight/2}, /**<center */
  {0,0}, {0,0},
  COLOR_BLUE,
  &fieldLayer,
};

//EDIT
Layer layerBall2 = { /**layer with moving ball */
  (AbShape *)&circle8,
  {(screenWidth/2), (screenHeight/2)+5},
   {0,0}, {0,0},
  COLOR_WHITE,
  &divisionLayer,  
};

Layer layerPaddle = { /* layer with paddle */
  (AbShape *)&Rect10,
  {screenWidth-110, (screenHeight/2)+5},
  //{screenWidth/2, screenHeight-18},
  {0,0}, {0,0},
  COLOR_PINK,
  &layerBall2,
};

Layer layerLeftPaddle = { /* layer with paddle */
  (AbShape *)&Rect10,
  {screenWidth-18, (screenHeight/2)+5},
  {0,0}, {0,0},
  COLOR_PINK,
  &layerPaddle,
};

//EDIT
Layer layerBall = { /**layer with moving ball */
  (AbShape *)&circle8,
  {(screenWidth/2 + 30), (screenHeight/2)+5},
   {0,0}, {0,0},
  COLOR_GREEN,
  &layerLeftPaddle,
};

/** Moving Layer
 *  Linked list of layer references
 *  Velocity represents one iteration of change (direction & magnitude)
 */

typedef struct MovLayer_s {
  Layer *layer;
  Vec2 velocity;
  struct MovLayer_s *next;
} MovLayer;

MovLayer rpaddle = {&layerPaddle, {1,1}, 0};
MovLayer lpaddle = {&layerLeftPaddle, {1,2}, &rpaddle};
MovLayer ball = {&layerBall, {2,1}, &lpaddle};

void movLayerDraw(MovLayer *movLayers, Layer *layers){
  int row, col;
  MovLayer *movLayer;

  and_sr(~8);
  for (movLayer = movLayers; movLayer; movLayer = movLayer->next){
    Layer *l = movLayer->layer;
    l->posLast = l->pos;
    l->pos = l->posNext;
  }
  or_sr(8);        /** disable interrupts (GIE on) */

    for (movLayer = movLayers; movLayer; movLayer = movLayer->next) { /* for each moving layer */
    Region bounds;
    layerGetBounds(movLayer->layer, &bounds);
    lcd_setArea(bounds.topLeft.axes[0], bounds.topLeft.axes[1], 
		bounds.botRight.axes[0], bounds.botRight.axes[1]);
    for (row = bounds.topLeft.axes[1]; row <= bounds.botRight.axes[1]; row++) {
      for (col = bounds.topLeft.axes[0]; col <= bounds.botRight.axes[0]; col++) {
	Vec2 pixelPos = {col, row};
	u_int color = bgColor;
	Layer *probeLayer;
	for (probeLayer = layers; probeLayer; 
	     probeLayer = probeLayer->next) { /* probe all layers, in order */
	  if (abShapeCheck(probeLayer->abShape, &probeLayer->pos, &pixelPos)) {
	    color = probeLayer->color;
	    break; 
	  } /* if probe check */
	} // for checking all layers at col, row
	lcd_writeColor(color); 
      } // for col
    } // for row
  } // for moving layer being updated
}

//Region fence = {{10,30}, {SHORT_EDGE_PIXELS-10, LONG_EDGE_PIXELS-10}}; /**< Create a fence region */

/** Advances a moving shape within a fence
 *  
 *  \param ml The moving shape to be advanced
 *  \param fence The region which will serve as a boundary for ml
 */
void mlAdvance(MovLayer *ml, Region *fence)
{

  drawString5x7(50,20, "Score: ", COLOR_RED, COLOR_BLACK);

  Vec2 newPos;
  u_char axis;
  Region shapeBoundary;

  for (; ml; ml = ml->next) {
    vec2Add(&newPos, &ml->layer->posNext, &ml->velocity);
    abShapeGetBounds(ml->layer->abShape, &newPos, &shapeBoundary);
    for (axis = 0; axis < 2; axis ++) {
       //if collision with border, end game
      if ((shapeBoundary.topLeft.axes[axis] < fence->topLeft.axes[axis]) ||
	  (shapeBoundary.botRight.axes[axis] > fence->botRight.axes[axis]) ) {
	//drawString5x7(50,20, "GAME OVER ", COLOR_RED, COLOR_BLACK);
	int velocity = ml->velocity.axes[axis] = -ml->velocity.axes[axis];
	newPos.axes[axis] += (2*velocity);
      }	/**< if outside of fence */
    } /**< for axis */
    ml->layer->posNext = newPos;
  } /**< for ml */
}


u_int bgColor = COLOR_BLACK;     /**< The background color */
int redrawScreen = 1;            /**< Boolean for whether screen needs to be redrawn */

Region fieldFence;		/**< fence around playing field  */


/** Initializes everything, enables interrupts and green LED, 
 *  and handles the rendering for the screen
 */
void main()
{
  P1DIR |= GREEN_LED;		/**< Green led on when CPU on */	       
  P1OUT |= GREEN_LED;

  configureClocks();
  lcd_init();
  shapeInit();
  p2sw_init(1);
  shapeInit();
 
  //layerInit(&layerLeftPaddle);
  //layerDraw(&layerLeftPaddle);
  layerInit(&layerBall);
  layerDraw(&layerBall);

  layerGetBounds(&fieldLayer, &fieldFence);

  enableWDTInterrupts();      /**< enable periodic interrupt */
  or_sr(0x8);	              /**< GIE (enable interrupts) */

  drawString5x7(50,20, "Score: ", COLOR_RED, COLOR_BLACK);

  for(;;) { 
    while (!redrawScreen) { /**< Pause CPU if screen doesn't need updating */
      P1OUT &= ~GREEN_LED;    /**< Green led off witHo CPU */
      or_sr(0x10);	      /**< CPU OFF */
    }
    P1OUT |= GREEN_LED;       /**< Green led on when CPU on */
    redrawScreen = 0;
    movLayerDraw(&ball, &layerBall);
  }
}

void movePaddles(MovLayer *movLayers){

}


/** Watchdog timer interrupt handler. 15 interrupts/sec */
void wdt_c_handler()
{
  static short count = 0;
  P1OUT |= GREEN_LED;		      /**< Green LED on when cpu on */
  count ++;
  if (count == 15) {
    mlAdvance(&ball, &fieldFence);
    //if (p2sw_read())
    redrawScreen = 1;
    count = 0;
  } 
  P1OUT &= ~GREEN_LED;		    /**< Green LED off when cpu off */
}
