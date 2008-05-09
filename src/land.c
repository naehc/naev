/*
 * See Licensing and Copyright notice in naev.h
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "land.h"

#include "naev.h"
#include "log.h"
#include "toolkit.h"
#include "player.h"
#include "rng.h"
#include "music.h"
#include "economy.h"
#include "hook.h"
#include "mission.h"
#include "ntime.h"
#include "save.h"
#include "music.h"


/* global/main window */
#define LAND_WIDTH   700
#define LAND_HEIGHT  600
#define BUTTON_WIDTH 200
#define BUTTON_HEIGHT 40

/* commodity window */
#define COMMODITY_WIDTH 400
#define COMMODITY_HEIGHT 400

/* outfits */
#define OUTFITS_WIDTH   700
#define OUTFITS_HEIGHT  600

/* shipyard */
#define SHIPYARD_WIDTH  700
#define SHIPYARD_HEIGHT 600

/* news window */
#define NEWS_WIDTH   400
#define NEWS_HEIGHT  500

/* bar window */
#define BAR_WIDTH    460
#define BAR_HEIGHT   300

/* mission computer window */
#define MISSION_WIDTH   700
#define MISSION_HEIGHT  600


/*
 * we use visited flags to not duplicate missions generated
 */
#define VISITED_LAND          (1<<0)
#define VISITED_COMMODITY     (1<<1)
#define VISITED_BAR           (1<<2)
#define VISITED_OUTFITS       (1<<3)
#define VISITED_SHIPYARD      (1<<4)
#define visited(f)            (land_visited |= (f))
#define has_visited(f)        (land_visited & (f))
static unsigned int land_visited = 0;



/*
 * land variables
 */
int landed = 0;
Planet* land_planet = NULL;

/*
 * mission computer stack
 */
static Mission* mission_computer = NULL;
static int mission_ncomputer = 0;

/*
 * player stuff
 */
extern int hyperspace_target;

/*
 * window stuff
 */
static int land_wid = 0; /* used for the primary land window */
static int secondary_wid = 0; /* used for the second opened land window */
static int terciary_wid = 0; /* used for fancy things like news, your ships... */


/*
 * prototypes
 */
/* commodity exchange */
static void commodity_exchange (void);
static void commodity_exchange_close( char* str );
static void commodity_update( char* str );
static void commodity_buy( char* str );
static void commodity_sell( char* str );
/* outfits */
static void outfits (void);
static void outfits_close( char* str );
static void outfits_update( char* str );
static int outfit_canBuy( Outfit* outfit, int q, int errmsg );
static void outfits_buy( char* str );
static int outfit_canSell( Outfit* outfit, int q, int errmsg );
static void outfits_sell( char* str );
static int outfits_getMod (void);
static void outfits_renderMod( double bx, double by, double w, double h );
/* shipyard */
static void shipyard (void);
static void shipyard_close( char* str );
static void shipyard_update( char* str );
static void shipyard_info( char* str );
static void shipyard_buy( char* str );
/* your ships */
static void shipyard_yours( char* str );
static void shipyard_yoursClose( char* str );
static void shipyard_yoursUpdate( char* str );
static void shipyard_yoursChange( char* str );
static void shipyard_yoursTransport( char* str );
static int shipyard_yoursTransportPrice( char* shipname );
/* spaceport bar */
static void spaceport_bar (void);
static void spaceport_bar_close( char* str );
/* news */
static void news (void);
static void news_close( char* str );
/* mission computer */
static void misn (void);
static void misn_close( char* str );
static void misn_accept( char* str );
static void misn_genList( int first );
static void misn_update( char* str );
/* refuel */
static int refuel_price (void);
static void spaceport_refuel( char *str );


/*
 * the local market
 */
static void commodity_exchange (void)
{
   int i;
   char **goods;

   /* window */
   secondary_wid = window_create( "Commodity Exchange",
         -1, -1, COMMODITY_WIDTH, COMMODITY_HEIGHT );

   /* buttons */
   window_addButton( secondary_wid, -20, 20,
         BUTTON_WIDTH, BUTTON_HEIGHT, "btnCommodityClose",
         "Close", commodity_exchange_close );
   window_addButton( secondary_wid, -40-((BUTTON_WIDTH-20)/2), 20*2 + BUTTON_HEIGHT,
         (BUTTON_WIDTH-20)/2, BUTTON_HEIGHT, "btnCommodityBuy",
         "Buy", commodity_buy );
   window_addButton( secondary_wid, -20, 20*2 + BUTTON_HEIGHT,
         (BUTTON_WIDTH-20)/2, BUTTON_HEIGHT, "btnCommoditySell",
         "Sell", commodity_sell );

   /* text */
   window_addText( secondary_wid, -20, -40, BUTTON_WIDTH, 60, 0,
         "txtSInfo", &gl_smallFont, &cDConsole, 
         "You have:\n"
         "Market price:\n"
         "\n"
         "Free Space:\n" );
   window_addText( secondary_wid, -20, -40, BUTTON_WIDTH/2, 60, 0,
         "txtDInfo", &gl_smallFont, &cBlack, NULL );
   window_addText( secondary_wid, -40, -100, BUTTON_WIDTH-20,
         BUTTON_WIDTH, 0, "txtDesc", &gl_smallFont, &cBlack, NULL );

   /* goods list */
   goods = malloc(sizeof(char*) * land_planet->ncommodities);
   for (i=0; i<land_planet->ncommodities; i++)
      goods[i] = strdup(land_planet->commodities[i]->name);
   window_addList( secondary_wid, 20, -40,
         COMMODITY_WIDTH-BUTTON_WIDTH-60, COMMODITY_HEIGHT-80-BUTTON_HEIGHT,
         "lstGoods", goods, land_planet->ncommodities, 0, commodity_update );

   /* update */
   commodity_update(NULL);

   if (!has_visited(VISITED_COMMODITY)) {
      /* TODO mission check */
      visited(VISITED_COMMODITY);
   }
}
static void commodity_exchange_close( char* str )
{
   if (strcmp(str, "btnCommodityClose")==0)
      window_destroy(secondary_wid);
   secondary_wid = 0;
}
static void commodity_update( char* str )
{
   (void)str;
   char buf[128];
   char *comname;
   Commodity *com;

   comname = toolkit_getList( secondary_wid, "lstGoods" );
   com = commodity_get( comname );

   /* modify text */
   snprintf( buf, 128,
         "%d tons\n"
         "%d credits/ton\n"
         "\n"
         "%d tons\n",
         player_cargoOwned( comname ),
         com->medium,
         pilot_cargoFree(player));
   window_modifyText( secondary_wid, "txtDInfo", buf );
   window_modifyText( secondary_wid, "txtDesc", com->description );

}
static void commodity_buy( char* str )
{
   (void)str;
   char *comname;
   Commodity *com;
   int q;

   q = 10;
   comname = toolkit_getList( secondary_wid, "lstGoods" );
   com = commodity_get( comname );

   if (player->credits <= q * com->medium) {
      dialogue_alert( "Not enough credits!" );
      return;
   }
   else if (pilot_cargoFree(player) <= 0) {
      dialogue_alert( "Not enough free space!" );
      return;
   }

   q = pilot_addCargo( player, com, q );
   player->credits -= q * com->medium;
   commodity_update(NULL);
}
static void commodity_sell( char* str )
{
   (void)str;
   char *comname;
   Commodity *com;
   int q;

   q = 10;
   comname = toolkit_getList( secondary_wid, "lstGoods" );
   com = commodity_get( comname );

   q = pilot_rmCargo( player, com, q );
   player->credits += q * com->medium;
   commodity_update(NULL);
}


/*
 * ze outfits
 */
static void outfits (void)
{
   char **outfits;
   int noutfits;
   char buf[128];

   /* create window */
   snprintf(buf,128,"%s - Outfits", land_planet->name );
   secondary_wid = window_create( buf, -1, -1,
         OUTFITS_WIDTH, OUTFITS_HEIGHT );
   window_setFptr( secondary_wid, outfits_buy ); /* will allow buying from keyboard */

   /* buttons */
   window_addButton( secondary_wid, -20, 20,
         BUTTON_WIDTH, BUTTON_HEIGHT, "btnCloseOutfits",
         "Close", outfits_close );
   window_addButton( secondary_wid, -40-BUTTON_WIDTH, 40+BUTTON_HEIGHT,
         BUTTON_WIDTH, BUTTON_HEIGHT, "btnBuyOutfit",
         "Buy", outfits_buy );
   window_addButton( secondary_wid, -40-BUTTON_WIDTH, 20,
         BUTTON_WIDTH, BUTTON_HEIGHT, "btnSellOutfit",
         "Sell", outfits_sell );

   /* fancy 128x128 image */
   window_addRect( secondary_wid, -20, -50, 128, 128, "rctImage", &cBlack, 0 );
   window_addImage( secondary_wid, -20-128, -50-128, "imgOutfit", NULL, 1 );

   /* cust draws the modifier */
   window_addCust( secondary_wid, -40-BUTTON_WIDTH, 60+2*BUTTON_HEIGHT,
         BUTTON_WIDTH, BUTTON_HEIGHT, "cstMod", 0, outfits_renderMod, NULL );

   /* the descriptive text */
   window_addText( secondary_wid, 40+200+20, -60,
         80, 96, 0, "txtSDesc", &gl_smallFont, &cDConsole,
         "Name:\n"
         "Type:\n"
         "Owned:\n"
         "\n"
         "Space taken:\n"
         "Free Space:\n"
         "\n"
         "Price:\n"
         "Money:\n" );
   window_addText( secondary_wid, 40+200+40+60, -60,
         250, 96, 0, "txtDDesc", &gl_smallFont, &cBlack, NULL );
   window_addText( secondary_wid, 20+200+40, -220,
         OUTFITS_WIDTH-300, 180, 0, "txtDescription",
         &gl_smallFont, NULL, NULL );

   /* set up the outfits to buy/sell */
   outfits = outfit_getTech( &noutfits, land_planet->tech, PLANET_TECH_MAX);
   window_addList( secondary_wid, 20, 40,
         200, OUTFITS_HEIGHT-80, "lstOutfits",
         outfits, noutfits, 0, outfits_update );

   /* write the outfits stuff */
   outfits_update( NULL );

   if (!has_visited(VISITED_OUTFITS)) {
      /* TODO mission check */
      visited(VISITED_OUTFITS);
   }
}
static void outfits_close( char* str )
{
   if (strcmp(str,"btnCloseOutfits")==0)
      window_destroy(secondary_wid);
   secondary_wid = 0;
}
static void outfits_update( char* str )
{
   (void)str;
   char *outfitname;
   Outfit* outfit;
   char buf[128], buf2[16], buf3[16];

   outfitname = toolkit_getList( secondary_wid, "lstOutfits" );
   outfit = outfit_get( outfitname );

   /* new image */
   window_modifyImage( secondary_wid, "imgOutfit", outfit->gfx_store );

   if (outfit_canBuy(outfit,1,0) > 0)
      window_enableButton( secondary_wid, "btnBuyOutfit" );
   else
      window_disableButton( secondary_wid, "btnBuyOutfit" );

   /* gray out sell button */
   if (outfit_canSell(outfit,1,0) > 0)
      window_enableButton( secondary_wid, "btnSellOutfit" );
   else
      window_disableButton( secondary_wid, "btnSellOutfit" );

   /* new text */
   window_modifyText( secondary_wid, "txtDescription", outfit->description );
   credits2str( buf2, outfit->price, 2 );
   credits2str( buf3, player->credits, 2 );
   snprintf( buf, 128,
         "%s\n"
         "%s\n"
         "%d\n"
         "\n"
         "%d\n"
         "%d\n"
         "\n"
         "%s credits\n"
         "%s credits\n",
         outfit->name,
         outfit_getType(outfit),
         player_outfitOwned(outfitname),
         outfit->mass,
         pilot_freeSpace(player),
         buf2,
         buf3 );
   window_modifyText( secondary_wid,  "txtDDesc", buf );
}
static int outfit_canBuy( Outfit* outfit, int q, int errmsg )
{
   char buf[16];

   /* can player actually fit the outfit? */
   if ((pilot_freeSpace(player) - outfit->mass) < 0) {
      if (errmsg != 0)
         dialogue_alert( "Not enough free space (you need %d more).",
               outfit->mass - pilot_freeSpace(player) );
      return 0;
   }
   /* has too many already */
   else if (player_outfitOwned(outfit->name) >= outfit->max) {
      if (errmsg != 0)
         dialogue_alert( "You can only carry %d of this outfit.",
               outfit->max );
      return 0;
   }
   /* can only have one afterburner */
   else if (outfit_isAfterburner(outfit) && (player->afterburner!=NULL)) {
      if (errmsg != 0)
         dialogue_alert( "You can only have one afterburner." );
      return 0;
   }
   /* takes away cargo space but you don't have any */
   else if (outfit_isMod(outfit) && (outfit->u.mod.cargo < 0)
         && (pilot_cargoFree(player) < -outfit->u.mod.cargo)) {
      if (errmsg != 0)
         dialogue_alert( "You need to empty your cargo first." );
      return 0;
   }
   /* not enough $$ */
   else if (q*(int)outfit->price >= player->credits) {
      if (errmsg != 0) {
         credits2str( buf, q*outfit->price - player->credits, 2 );
         dialogue_alert( "You need %s more credits.", buf);
      }
      return 0;
   }

   return 1;
}
static void outfits_buy( char* str )
{
   (void)str;
   char *outfitname;
   Outfit* outfit;
   int q;

   outfitname = toolkit_getList( secondary_wid, "lstOutfits" );
   outfit = outfit_get( outfitname );

   q = outfits_getMod();

   /* can buy the outfit? */
   if (outfit_canBuy(outfit, q, 1) == 0) return;

   player->credits -= outfit->price * pilot_addOutfit( player, outfit,
         MIN(q,outfit->max) );
   outfits_update(NULL);
}
static int outfit_canSell( Outfit* outfit, int q, int errmsg )
{
   /* has no outfits to sell */
   if (player_outfitOwned(outfit->name) <= 0) {
      if (errmsg != 0)
         dialogue_alert( "You can't sell something you don't have." );
      return 0;
   }
   /* can't sell when you are using it */
   else if (outfit_isMod(outfit) && (pilot_cargoFree(player) < outfit->u.mod.cargo*q)) {
      if (errmsg != 0)
         dialogue_alert( "You currently have cargo in this modification." );
      return 0;
   }
   /* has negative mass and player has no free space */
   else if ((outfit->mass < 0) && (pilot_freeSpace(player) < -outfit->mass)) {
      if (errmsg != 0)
         dialogue_alert( "Get rid of other outfits first.");
      return 0;
   }

   return 1;
}
static void outfits_sell( char* str )
{
   (void)str;
   char *outfitname;
   Outfit* outfit;
   int q;

   outfitname = toolkit_getList( secondary_wid, "lstOutfits" );
   outfit = outfit_get( outfitname );

   q = outfits_getMod();

   /* has no outfits to sell */
   if (outfit_canSell( outfit, q, 1 ) == 0) return;

   player->credits += outfit->price * pilot_rmOutfit( player, outfit, q );
   outfits_update(NULL);
}
/*
 * returns the current modifier status
 */
static int outfits_getMod (void)
{
   SDLMod mods;
   int q;

   mods = SDL_GetModState();
   q = 1;
   if (mods & (KMOD_LCTRL | KMOD_RCTRL)) q *= 5;
   if (mods & (KMOD_LSHIFT | KMOD_RSHIFT)) q *= 10;

   return q;
}
static void outfits_renderMod( double bx, double by, double w, double h )
{
   (void) h;
   int q;
   char buf[8];

   q = outfits_getMod();
   if (q==1) return;

   snprintf( buf, 8, "%dx", q );
   gl_printMid( &gl_smallFont, w,
         bx + (double)SCREEN_W/2.,
         by + (double)SCREEN_H/2.,
         &cBlack, buf );
}




/*
 * ze shipyard
 */
static void shipyard (void)
{
   char **ships;
   int nships;
   char buf[128];

   /* window creation */
   snprintf( buf, 128, "%s - Shipyard", land_planet->name );
   secondary_wid = window_create( buf,
         -1, -1, SHIPYARD_WIDTH, SHIPYARD_HEIGHT );

   /* buttons */
   window_addButton( secondary_wid, -20, 20,
         BUTTON_WIDTH, BUTTON_HEIGHT, "btnCloseShipyard",
         "Close", shipyard_close );
   window_addButton( secondary_wid, -20, 40+BUTTON_HEIGHT,
         BUTTON_WIDTH, BUTTON_HEIGHT, "btnYourShips",
         "Your Ships", shipyard_yours );
   window_addButton( secondary_wid, -40-BUTTON_WIDTH, 20,
         BUTTON_WIDTH, BUTTON_HEIGHT, "btnBuyShip",
         "Buy", shipyard_buy );
   window_addButton( secondary_wid, -40-BUTTON_WIDTH, 40+BUTTON_HEIGHT,
         BUTTON_WIDTH, BUTTON_HEIGHT, "btnInfoShip",
         "Info", shipyard_info );

   /* target gfx */
   window_addRect( secondary_wid, -40, -50,
         128, 96, "rctTarget", &cBlack, 0 );
   window_addImage( secondary_wid, -40-128, -50-96,
         "imgTarget", NULL, 1 );

   /* text */
   window_addText( secondary_wid, 40+200+40, -55,
         80, 96, 0, "txtSDesc", &gl_smallFont, &cDConsole,
         "Name:\n"
         "Class:\n"
         "Fabricator:\n"
         "\n"
         "Price:\n"
         "Money:\n" );
   window_addText( secondary_wid, 40+200+40+80, -55,
         130, 96, 0, "txtDDesc", &gl_smallFont, &cBlack, NULL );
   window_addText( secondary_wid, 20+200+40, -160,
         SHIPYARD_WIDTH-300, 200, 0, "txtDescription",
         &gl_smallFont, NULL, NULL );

   /* set up the ships to buy/sell */
   ships = ship_getTech( &nships, land_planet->tech, PLANET_TECH_MAX );
   window_addList( secondary_wid, 20, 40,
         200, SHIPYARD_HEIGHT-80, "lstShipyard",
         ships, nships, 0, shipyard_update );

   /* write the shipyard stuff */
   shipyard_update( NULL );

   if (!has_visited(VISITED_SHIPYARD)) {
      /* TODO mission check */
      visited(VISITED_SHIPYARD);
   }
}
static void shipyard_close( char* str )
{
   if (strcmp(str,"btnCloseShipyard")==0)
      window_destroy(secondary_wid);
   secondary_wid = 0;
}
static void shipyard_update( char* str )
{
   (void)str;
   char *shipname;
   Ship* ship;
   char buf[80], buf2[16], buf3[16];
   
   shipname = toolkit_getList( secondary_wid, "lstShipyard" );
   ship = ship_get( shipname );

   /* toggle your shipyard */
   if (player_nships()==0) window_disableButton(secondary_wid,"btnYourShips");
   else window_enableButton(secondary_wid,"btnYourShips");

   /* update image */
   window_modifyImage( secondary_wid, "imgTarget", ship->gfx_target );

   /* update text */
   window_modifyText( secondary_wid, "txtDescription", ship->description );
   credits2str( buf2, ship->price, 2 );
   credits2str( buf3, player->credits, 2 );
   snprintf( buf, 80,
         "%s\n"
         "%s\n"
         "%s\n"
         "\n"
         "%s credits\n"
         "%s credits\n",
         ship->name,
         ship_class(ship),
         ship->fabricator,
         buf2,
         buf3);
   window_modifyText( secondary_wid,  "txtDDesc", buf );

   if (ship->price > player->credits)
      window_disableButton( secondary_wid, "btnBuyShip");
   else window_enableButton( secondary_wid, "btnBuyShip");
}
static void shipyard_info( char* str )
{
   (void)str;
   char *shipname;

   shipname = toolkit_getList( secondary_wid, "lstShipyard" );
   ship_view(shipname);
}
static void shipyard_buy( char* str )
{
   (void)str;
   char *shipname, buf[16];
   Ship* ship;

   shipname = toolkit_getList( secondary_wid, "lstShipyard" );
   ship = ship_get( shipname );

   /* we now move cargo to the new ship */
   if (pilot_cargoUsed(player) > ship->cap_cargo) {
      dialogue_alert("You won't have space to move your current cargo onto the new ship.");
      return; 
   }
   credits2str( buf, ship->price, 2 );
   if (dialogue_YesNo("Are you sure?", /* confirm */
         "Do you really want to spend %s on a new ship?", buf )==0)
      return;

   /* player just gots a new ship */
   player_newShip( ship, player->solid->pos.x, player->solid->pos.y,
         0., 0., player->solid->dir );
   player->credits -= ship->price; /* ouch, paying is hard */

   shipyard_update(NULL);
}
static void shipyard_yours( char* str )
{
   (void)str;
   char **ships;
   int nships;

   /* create window */
   terciary_wid = window_create( "Your Ships",
         -1, -1, SHIPYARD_WIDTH, SHIPYARD_HEIGHT );

   /* buttons */
   window_addButton( terciary_wid, -20, 20,
         BUTTON_WIDTH, BUTTON_HEIGHT, "btnCloseYourShips",
         "Shipyard", shipyard_yoursClose );
   window_addButton( terciary_wid, -40-BUTTON_WIDTH, 20,
         BUTTON_WIDTH, BUTTON_HEIGHT, "btnChangeShip",
         "Change Ship", shipyard_yoursChange );
   window_addButton( terciary_wid, -40-BUTTON_WIDTH, 40+BUTTON_HEIGHT,
         BUTTON_WIDTH, BUTTON_HEIGHT, "btnTransportShip",
         "Transport Ship", shipyard_yoursTransport );

   /* image */
   window_addRect( terciary_wid, -40, -50,
         128, 96, "rctTarget", &cBlack, 0 );
   window_addImage( terciary_wid, -40-128, -50-96,
         "imgTarget", NULL, 1 );

   /* text */
   window_addText( terciary_wid, 40+200+40, -55,
         100, 96, 0, "txtSDesc", &gl_smallFont, &cDConsole,
         "Name:\n"
         "Ship:\n"
         "Class:\n"
         "Where:\n"
         "\n"
         "Cargo free:\n"
         "Weapons free:\n"
         "\n"
         "Transportation:\n"
         "Sell price:\n"
         );
   window_addText( terciary_wid, 40+200+40+100, -55,
      130, 96, 0, "txtDDesc", &gl_smallFont, &cBlack, NULL );
   window_addText( terciary_wid, 40+200+40, -215,
      100, 20, 0, "txtSOutfits", &gl_smallFont, &cDConsole,
      "Outfits:\n"
      );
   window_addText( terciary_wid, 40+200+40, -215-gl_smallFont.h-5,
      SHIPYARD_WIDTH-40-200-40-20, 200, 0, "txtDOutfits",
      &gl_smallFont, &cBlack, NULL );

   /* ship list */
   ships = player_ships( &nships );
   window_addList( terciary_wid, 20, 40,
         200, SHIPYARD_HEIGHT-80, "lstYourShips",
         ships, nships, 0, shipyard_yoursUpdate );

   shipyard_yoursUpdate(NULL);
}
static void shipyard_yoursClose( char* str )
{
   (void)str;
   window_destroy( terciary_wid );
   terciary_wid = 0;
}
static void shipyard_yoursUpdate( char* str )
{
   (void)str;
   char buf[256], buf2[16], buf3[16], *buf4;
   char *shipname;
   Pilot *ship;
   char* loc;
   int price;

   shipname = toolkit_getList( terciary_wid, "lstYourShips" );
   if (strcmp(shipname,"None")==0) { /* no ships */
      window_disableButton( terciary_wid, "btnChangeShip" );
      window_disableButton( terciary_wid, "btnTransportShip" );
      return;
   }
   ship = player_getShip( shipname );
   loc = player_getLoc(ship->name);
   price = shipyard_yoursTransportPrice(shipname);

   /* update image */
   window_modifyImage( terciary_wid, "imgTarget", ship->ship->gfx_target );

   /* update text */
   credits2str( buf2, price , 2 ); /* transport */
   credits2str( buf3, 0, 2 ); /* sell price */
   snprintf( buf, 256,
         "%s\n"
         "%s\n"
         "%s\n"
         "%s\n"
         "\n"
         "%d tons\n"
         "%d tons\n"
         "\n"
         "%s credits\n"
         "%s credits\n",
         ship->name,
         ship->ship->name,
         ship_class(ship->ship),
         loc,
         pilot_cargoFree(ship),
         pilot_freeSpace(ship),
         buf2,
         buf3);
   window_modifyText( terciary_wid, "txtDDesc", buf );

   buf4 = pilot_getOutfits( ship );
   window_modifyText( terciary_wid, "txtDOutfits", buf4 );
   free(buf4);

   /* button disabling */
   if (strcmp(land_planet->name,loc)) { /* ship not here */
      window_disableButton( terciary_wid, "btnChangeShip" );
      if (price > player->credits)
         window_disableButton( terciary_wid, "btnTransportShip" );
      else window_enableButton( terciary_wid, "btnTransportShip" );
   }
   else { /* ship is here */
      window_enableButton( terciary_wid, "btnChangeShip" );
      window_disableButton( terciary_wid, "btnTransportShip" );
   }
}
static void shipyard_yoursChange( char* str )
{
   (void)str;
   char *shipname, *loc;
   Pilot *newship;

   shipname = toolkit_getList( terciary_wid, "lstYourShips" );
   newship = player_getShip(shipname);
   if (strcmp(shipname,"None")==0) { /* no ships */
      dialogue_alert( "You need another ship to change ships!" );
      return;
   }
   loc = player_getLoc(shipname);

   if (strcmp(loc,land_planet->name)) {
      dialogue_alert( "You must transport the ship to %s to be able to get in.",
            land_planet->name );
      return;
   }
   else if (pilot_cargoUsed(player) > pilot_cargoFree(newship)) {
      dialogue_alert( "You won't be able to fit your current cargo in the new ship." );
      return;
   }

   player_swapShip(shipname);

   /* recreate the window */
   shipyard_yoursClose(NULL);
   shipyard_yours(NULL);
}
static void shipyard_yoursTransport( char* str )
{
   (void)str;
   int price;
   char *shipname, buf[16];

   shipname = toolkit_getList( terciary_wid, "lstYourShips" );
   if (strcmp(shipname,"None")==0) { /* no ships */
      dialogue_alert( "You can't transport nothing here!" );
      return;
   }

   price = shipyard_yoursTransportPrice( shipname );
   if (price==0) { /* already here */
      dialogue_alert( "Your ship '%s' is already here.", shipname );
      return;
   }
   else if (player->credits < price) { /* not enough money */
      credits2str( buf, price-player->credits, 2 );
      dialogue_alert( "You need %d more credits to transport '%s' here.",
            buf, shipname );
      return;
   }

   /* success */
   player->credits -= price;
   player_setLoc( shipname, land_planet->name );

   /* update the window to reflect the change */
   shipyard_yoursUpdate(NULL);
}
static int shipyard_yoursTransportPrice( char* shipname )
{
   char *loc;
   Pilot* ship;
   int price;

   ship = player_getShip(shipname);
   loc = player_getLoc(shipname);
   if (strcmp(loc,land_planet->name)==0) /* already here */
      return 0;

   price = (int)ship->solid->mass*500;

   return price;
}


/*
 * the spaceport bar
 */
static void spaceport_bar (void)
{
   /* window */
   secondary_wid = window_create( "Spaceport Bar",
         -1, -1, BAR_WIDTH, BAR_HEIGHT );

   /* buttons */
   window_addButton( secondary_wid, -20, 20,
         BUTTON_WIDTH, BUTTON_HEIGHT, "btnCloseBar",
         "Close", spaceport_bar_close );
   window_addButton( secondary_wid, 20, 20,
         BUTTON_WIDTH, BUTTON_HEIGHT, "btnNews",
         "News", (void(*)(char*))news);

   /* text */
   window_addText( secondary_wid, 20, -50,
         BAR_WIDTH - 40, BAR_HEIGHT - 60 - BUTTON_HEIGHT, 0,
         "txtDescription", &gl_smallFont, &cBlack, land_planet->bar_description );

   if (!has_visited(VISITED_BAR)) {
      missions_bar(land_planet->faction, land_planet->name, cur_system->name);
      visited(VISITED_BAR);
   }
}
static void spaceport_bar_close( char* str )
{
   if (strcmp(str,"btnCloseBar")==0)
      window_destroy(secondary_wid);
   secondary_wid = 0;
}



/*
 * the planetary news reports
 */
static void news (void)
{
   /* create window */
   terciary_wid = window_create( "News Reports",
         -1, -1, NEWS_WIDTH, NEWS_HEIGHT );

   /* buttons */
   window_addButton( terciary_wid, -20, 20,
         BUTTON_WIDTH, BUTTON_HEIGHT, "btnCloseNews",
         "Close", news_close );

   /* text */
   window_addText( terciary_wid, 20, 20 + BUTTON_HEIGHT + 20,
         NEWS_WIDTH-40, NEWS_HEIGHT - 20 - BUTTON_HEIGHT - 20 - 20 - 20,
         0, "txtNews", &gl_smallFont, &cBlack,
         "News reporters report that they are on strike!");
}
static void news_close( char* str )
{
   if (strcmp(str,"btnCloseNews")==0)
      window_destroy( terciary_wid );
   terciary_wid = 0;
}


/*
 * mission computer, cuz missions rock
 */
static void misn (void)
{
   /* create window */
   secondary_wid = window_create( "Mission Computer",
         -1, -1, MISSION_WIDTH, MISSION_HEIGHT );

   /* buttons */
   window_addButton( secondary_wid, -20, 20,
         BUTTON_WIDTH, BUTTON_HEIGHT, "btnCloseMission",
         "Close", misn_close );
   window_addButton( secondary_wid, -20, 40+BUTTON_HEIGHT,
         BUTTON_WIDTH, BUTTON_HEIGHT, "btnAcceptMission",
         "Accept", misn_accept );

   /* text */
   window_addText( secondary_wid, 300+40, -60,
         300, 40, 0, "txtSReward",
         &gl_smallFont, &cDConsole, "Reward:" );
   window_addText( secondary_wid, 300+100, -60,
         240, 40, 0, "txtReward", &gl_smallFont, &cBlack, NULL );
   window_addText( secondary_wid, 300+40, -100,
         300, MISSION_HEIGHT - BUTTON_WIDTH - 120, 0,
         "txtDesc", &gl_smallFont, &cBlack, NULL );

   misn_genList(1);
}
static void misn_close( char* str )
{
   if (strcmp(str,"btnCloseMission")==0)
      window_destroy( secondary_wid );
   secondary_wid = 0;
}
static void misn_accept( char* str )
{
   char* misn_name;
   Mission* misn;
   int pos;
   (void)str;

   misn_name = toolkit_getList( secondary_wid, "lstMission" );

   if (strcmp(misn_name,"No Missions")==0) return;

   if (dialogue_YesNo("Accept Mission",
         "Are you sure you want to accept this mission?")) {
      pos = toolkit_getListPos( secondary_wid, "lstMission" );
      misn = &mission_computer[pos];
      if (mission_accept( misn )) { /* successs in accepting the mission */
         memmove( misn, &mission_computer[pos+1],
               sizeof(Mission) * (mission_ncomputer-pos-1) );
         mission_ncomputer--;
         misn_genList(0);
      }
   }
}
static void misn_genList( int first )
{
   int i,j;
   char** misn_names;

   if (!first)
      window_destroyWidget( secondary_wid, "lstMission" );

   /* list */
   if (mission_ncomputer!=0) { /* there are missions */
      misn_names = malloc(sizeof(char*) * mission_ncomputer);
      j = 0;
      for (i=0; i<mission_ncomputer; i++)
         if (mission_computer[i].title)
            misn_names[j++] = strdup(mission_computer[i].title);
   }
   if ((mission_ncomputer==0) || (j==0)) { /* no missions */
      if (j==0) free(misn_names);
      misn_names = malloc(sizeof(char*));
      misn_names[0] = strdup("No Missions");
      j = 1;
   }
   window_addList( secondary_wid, 20, -40,
         300, MISSION_HEIGHT-60,
         "lstMission", misn_names, j, 0, misn_update );

   misn_update(NULL);
}
static void misn_update( char* str )
{
   char *active_misn;
   Mission* misn;

   (void)str;

   active_misn = toolkit_getList( secondary_wid, "lstMission" );
   if (strcmp(active_misn,"No Missions")==0) {
      window_modifyText( secondary_wid, "txtReward", "None" );
      window_modifyText( secondary_wid, "txtDesc",
            "There are no missions available here." );
      window_disableButton( secondary_wid, "btnAcceptMission" );
      return;
   }

   misn = &mission_computer[ toolkit_getListPos( secondary_wid, "lstMission" ) ];
   window_modifyText( secondary_wid, "txtReward", misn->reward );
   window_modifyText( secondary_wid, "txtDesc", misn->desc );
   window_enableButton( secondary_wid, "btnAcceptMission" );
}


/*
 * returns how much it will cost to refuel the player
 */
static int refuel_price (void)
{
   return (player->fuel_max - player->fuel)*3;
}


/*
 * refuel the player
 */
static void spaceport_refuel( char *str )
{
   (void)str;

   if (player->credits < refuel_price()) { /* player is out of money after landing */
      dialogue_alert("You seem to not have enough credits to refuel your ship" );
      return;
   }

   player->credits -= refuel_price();
   player->fuel = player->fuel_max;
   window_destroyWidget( land_wid, "btnRefuel" );
}


/*
 * lands the player
 */
void land( Planet* p )
{
   char buf[32], cred[16];

   if (landed) return;

   /* change music */
   music_choose("land");

   land_planet = p;
   land_wid = window_create( p->name, -1, -1, LAND_WIDTH, LAND_HEIGHT );
   
   /*
    * pretty display
    */
   window_addImage( land_wid, 20, -40, "imgPlanet", p->gfx_exterior, 1 );
   window_addText( land_wid, 440, 80, LAND_WIDTH-460, 460, 0, 
         "txtPlanetDesc", &gl_smallFont, &cBlack, p->description);

   /*
    * buttons
    */
   /* first column */
   window_addButton( land_wid, -20, 20,
         BUTTON_WIDTH, BUTTON_HEIGHT, "btnTakeoff",
         "Takeoff", (void(*)(char*))takeoff );
   if (planet_hasService(land_planet, PLANET_SERVICE_COMMODITY))
      window_addButton( land_wid, -20, 20 + BUTTON_HEIGHT + 20,
            BUTTON_WIDTH, BUTTON_HEIGHT, "btnCommodity",
            "Commodity Exchange", (void(*)(char*))commodity_exchange);
   /* second column */
   if (planet_hasService(land_planet, PLANET_SERVICE_SHIPYARD))
      window_addButton( land_wid, -20 - BUTTON_WIDTH - 20, 20,
            BUTTON_WIDTH, BUTTON_HEIGHT, "btnShipyard",
            "Shipyard", (void(*)(char*))shipyard);
   if (planet_hasService(land_planet, PLANET_SERVICE_OUTFITS))
      window_addButton( land_wid, -20 - BUTTON_WIDTH - 20, 20 + BUTTON_HEIGHT + 20,
            BUTTON_WIDTH, BUTTON_HEIGHT, "btnOutfits",
            "Outfits", (void(*)(char*))outfits);
   /* third column */
   if (planet_hasService(land_planet, PLANET_SERVICE_BASIC)) {
      window_addButton( land_wid, 20, 20,
            BUTTON_WIDTH, BUTTON_HEIGHT, "btnNews",
            "Mission Terminal", (void(*)(char*))misn);
      window_addButton( land_wid, 20, 20 + BUTTON_HEIGHT + 20,
            BUTTON_WIDTH, BUTTON_HEIGHT, "btnBar",
            "Spaceport Bar", (void(*)(char*))spaceport_bar);
      if (player->fuel < player->fuel_max) {
         credits2str( cred, refuel_price(), 2 );
         snprintf( buf, 32, "Refuel %s", cred );
         window_addButton( land_wid, -20, 20 + 2*(BUTTON_HEIGHT + 20),
               BUTTON_WIDTH, BUTTON_HEIGHT, "btnRefuel",
               buf, spaceport_refuel );
         if (player->credits < refuel_price()) /* not enough money */
            window_disableButton( land_wid, "btnRefuel" );
      }
   }


   /* player is now officially landed */
   landed = 1;
   hooks_run("land");

   /* generate mission computer stuff */
   mission_computer = missions_computer( &mission_ncomputer,
         land_planet->faction, land_planet->name, cur_system->name );

   if (!has_visited(VISITED_LAND)) {
      /* TODO mission check */
      visited(VISITED_LAND);
   }
}


/*
 * takes off the player
 */
void takeoff (void)
{
   int sw,sh, i, h;
   char *nt;

   if (!landed) return;

   /* ze music */
   music_choose("takeoff");

   /* to randomize the takeoff a bit */
   sw = land_planet->gfx_space->w;
   sh = land_planet->gfx_space->h;

   /* no longer authorized to land */
   player_rmFlag(PLAYER_LANDACK);

   /* set player to another position with random facing direction and no vel */
   player_warp( land_planet->pos.x + RNG(-sw/2,sw/2),
         land_planet->pos.y + RNG(-sh/2,sh/2) );
   vect_pset( &player->solid->vel, 0., 0. );
   player->solid->dir = RNG(0,359) * M_PI/180.;

   /* heal the player */
   player->armour = player->armour_max;
   player->shield = player->shield_max;
   player->energy = player->energy_max;

   /* time goes by, triggers hook before takeoff */
   ntime_inc( RNG( 2*NTIME_UNIT_LENGTH, 3*NTIME_UNIT_LENGTH ) );
   nt = ntime_pretty(0);
   player_message("Taking off from %s on %s", land_planet->name, nt);
   free(nt);

   /* initialize the new space */
   h = hyperspace_target;
   space_init(NULL);
   hyperspace_target = h;

   /* cleanup */
   save_all(); /* must be before cleaning up planet */
   land_planet = NULL;
   window_destroy( land_wid );
   landed = 0;
   land_visited = 0;
   hooks_run("takeoff"); 
   hooks_run("enter");

   /* cleanup mission computer */
   for (i=0; i<mission_ncomputer; i++)
      mission_cleanup( &mission_computer[i] );
   free(mission_computer);
   mission_computer = NULL;
   mission_ncomputer = 0;
}

