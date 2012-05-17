/*
 * PyEPG grabber
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "htsmsg_xml.h"
#include "tvheadend.h"
#include "spawn.h"
#include "epg.h"
#include "epggrab/pyepg.h"

void   _pyepg_grab            ( const char **argv );
void   _pyepg_parse           ( htsmsg_t *data );
void   _pyepg_parse_epg       ( htsmsg_t *data );
int    _pyepg_parse_channel   ( htsmsg_t *data );
int    _pyepg_parse_brand     ( htsmsg_t *data );
int    _pyepg_parse_season    ( htsmsg_t *data );
int    _pyepg_parse_episode   ( htsmsg_t *data );
int    _pyepg_parse_broadcast ( htsmsg_t *data, epg_channel_t *channel );
int    _pyepg_parse_schedule  ( htsmsg_t *data );
int    _pyepg_parse_time      ( const char *str, time_t *tm );

/* ************************************************************************
 * Module Setup
 * ***********************************************************************/

static epggrab_module_t pyepg_module;

static const char* pyepg_name ( void )
{
  return "pyepg";
}

static void pyepg_run ( const char *iopts )
{
  int i = 1;
  const char *argv[32]; // 32 args max!
  char *toksave, *tok;
  char *opts = strdup(iopts);

  /* TODO: do something better! */
  argv[0] = "/usr/bin/pyepg";
  if ( opts ) {
    tok = strtok_r(opts, " ", &toksave);
    while ( tok != NULL ) {
      argv[i++] = tok;
      tok = strtok_r(NULL, " ", &toksave);
    }
    argv[i] = NULL;
  }

  _pyepg_grab(argv);
  free(opts);
}

epggrab_module_t* pyepg_init ( void )
{
  pyepg_module.enable  = NULL;
  pyepg_module.disable = NULL;
  pyepg_module.name    = pyepg_name;
  pyepg_module.run     = pyepg_run;
  return &pyepg_module;
}

/* **************************************************************************
 * Grabber
 * *************************************************************************/

void _pyepg_grab ( const char **argv )
{
  int      i = 0, outlen;
  char     *outbuf;
  char     cmdstr[1024], errbuf[100];
  time_t   t1, t2;
  htsmsg_t *body;

  /* Debug */
  cmdstr[0] = '\0';
  while ( argv[i] ) {
    strcat(cmdstr, argv[i++]);
    if ( argv[i] ) strcat(cmdstr, " ");
  }
  tvhlog(LOG_DEBUG, "pyepg", "grab %s", cmdstr); 

  /* Grab */
  time(&t1);
  outlen = spawn_and_store_stdout(argv[0], (char *const*)argv, &outbuf);
  if ( outlen < 1 ) {
    tvhlog(LOG_ERR, "pyepg", "no output detected");
    return;
  }
  time(&t2);
  tvhlog(LOG_DEBUG, "pyepg", "grab took %d seconds", t2 - t1);

  /* Extract */
  body = htsmsg_xml_deserialize(outbuf, errbuf, sizeof(errbuf));
  if ( !body ) {
    tvhlog(LOG_ERR, "pyepg", "unable to parse output [e=%s]", errbuf);
    return;
  }

  /* Parse */
  pthread_mutex_lock(&global_lock);
  _pyepg_parse(body);
  pthread_mutex_unlock(&global_lock);
  htsmsg_destroy(body);
}

/* **************************************************************************
 * Parsing
 * *************************************************************************/

void _pyepg_parse ( htsmsg_t *data )
{
  htsmsg_t *tags, *epg;

  if ((tags = htsmsg_get_map(data, "tags")) == NULL) return;
  
  /* PyEPG format */
  if ((epg = htsmsg_get_map(tags, "epg")) != NULL) _pyepg_parse_epg(epg);

  /* XMLTV format */
  if ((epg = htsmsg_get_map(tags, "tv")) != NULL) return;// TODO: add
}

int _pyepg_parse_channel ( htsmsg_t *data )
{
  int save = 0;
  htsmsg_t *attr, *tags;
  const char *id, *name = NULL;
  epg_channel_t *channel;

  if ( data == NULL ) return 0;

  if ((attr = htsmsg_get_map(data, "attrib")) == NULL) return 0;
  if ((id   = htsmsg_get_str(attr, "id")) == NULL) return 0;
  if ((tags = htsmsg_get_map(data, "tags")) == NULL) return 0;

  /* Find channel */
  if ((channel = epg_channel_find(id, name, NULL, NULL)) == NULL) return 0;
  // TODO: need to save if created

  return save;
}

int _pyepg_parse_brand ( htsmsg_t *data )
{
  int save = 0;
  htsmsg_t *attr, *tags;
  epg_brand_t *brand;
  const char *str;
  uint32_t u32;

  if ( data == NULL ) return 0;

  if ((attr = htsmsg_get_map(data, "attrib")) == NULL) return 0;
  if ((str  = htsmsg_get_str(attr, "id")) == NULL) return 0;
  if ((tags = htsmsg_get_map(data, "tags")) == NULL) return 0;
  
  /* Find brand */
  if ((brand = epg_brand_find_by_id(str, 1)) == NULL) return 0;
  // TODO: do we need to save if created?

  /* Set title */
  if ((str = htsmsg_xml_get_cdata_str(tags, "title"))) {
    save |= epg_brand_set_title(brand, str);
  }

  /* Set summary */
  if ((str = htsmsg_xml_get_cdata_str(tags, "summary"))) {
    save |= epg_brand_set_summary(brand, str);
  }
  
  /* Set icon */
#if TODO
  if ((str = htsmsg_xml_get_cdata_str(tags, "icon"))) {
    save |= epg_brand_set_icon(brand, str);
  }
#endif

  /* Set season count */
  if (htsmsg_xml_get_cdata_u32(tags, "series-count", &u32)) {
    save |= epg_brand_set_season_count(brand, u32);
  }

  return save;
}

int _pyepg_parse_season ( htsmsg_t *data )
{
  int save = 0;
  htsmsg_t *attr, *tags;
  epg_season_t *season;
  epg_brand_t *brand;
  const char *str;
  uint32_t u32;

  if ( data == NULL ) return 0;

  if ((attr = htsmsg_get_map(data, "attrib")) == NULL) return 0;
  if ((str  = htsmsg_get_str(attr, "id")) == NULL) return 0;
  if ((tags = htsmsg_get_map(data, "tags")) == NULL) return 0;

  /* Find series */
  if ((season = epg_season_find_by_id(str, 1)) == NULL) return 0;
  // TODO: do we need to save if created?
  
  /* Set brand */
  if ((str = htsmsg_xml_get_cdata_str(tags, "brand"))) {
    if ((brand = epg_brand_find_by_id(str, 0))) {
      save |= epg_season_set_brand(season, brand);
    }
  }

  /* Set title */
#if TODO
  if ((str = htsmsg_xml_get_cdata_str(tags, "title"))) {
    save |= epg_season_set_title(season, str);
  } 

  /* Set summary */
  if ((str = htsmsg_xml_get_cdata_str(tags, "summary"))) {
    save |= epg_season_set_summary(season, str);
  }
  
  /* Set icon */
  if ((str = htsmsg_xml_get_cdata_str(tags, "icon"))) {
    save |= epg_season_set_icon(season, str);
  }
#endif

  /* Set season number */
  if (htsmsg_xml_get_cdata_u32(tags, "number", &u32)) {
    save |= epg_season_set_number(season, u32);
  }

  /* Set episode count */
  if (htsmsg_xml_get_cdata_u32(tags, "episode-count", &u32)) {
    save |= epg_season_set_episode_count(season, u32);
  }

  return save;
}

int _pyepg_parse_episode ( htsmsg_t *data )
{
  int save = 0;
  htsmsg_t *attr, *tags;
  epg_episode_t *episode;
  epg_season_t *season;
  epg_brand_t *brand;
  const char *str;
  uint32_t u32;

  if ( data == NULL ) return 0;

  if ((attr = htsmsg_get_map(data, "attrib")) == NULL) return 0;
  if ((str  = htsmsg_get_str(attr, "id")) == NULL) return 0;
  if ((tags = htsmsg_get_map(data, "tags")) == NULL) return 0;

  /* Find episode */
  if ((episode = epg_episode_find_by_id(str, 1)) == NULL) return 0;
  // TODO: do we need to save if created?
  
  /* Set brand */
  if ((str = htsmsg_xml_get_cdata_str(tags, "brand"))) {
    if ((brand = epg_brand_find_by_id(str, 0))) {
      save |= epg_episode_set_brand(episode, brand);
    }
  }

  /* Set season */
  if ((str = htsmsg_xml_get_cdata_str(tags, "season"))) {
    if ((season = epg_season_find_by_id(str, 0))) {
      save |= epg_episode_set_season(episode, season);
    }
  }

  /* Set title/subtitle */
  if ((str = htsmsg_xml_get_cdata_str(tags, "title"))) {
    save |= epg_episode_set_title(episode, str);
  } 
  if ((str = htsmsg_xml_get_cdata_str(tags, "subtitle"))) {
    save |= epg_episode_set_subtitle(episode, str);
  } 

  /* Set summary */
  if ((str = htsmsg_xml_get_cdata_str(tags, "summary"))) {
    save |= epg_episode_set_summary(episode, str);
  }

  /* Number */
  if (htsmsg_xml_get_cdata_u32(tags, "number", &u32)) {
    save |= epg_episode_set_number(episode, u32);
  }

  /* Genre */
  // TODO: can actually have multiple!
#if TODO
  if ((str = htsmsg_xml_get_cdata_str(tags, "genre"))) {
    // TODO: conversion?
    save |= epg_episode_set_genre(episode, str);
  }
#endif

  /* TODO: extra metadata */

  return save;
}

int _pyepg_parse_broadcast ( htsmsg_t *data, epg_channel_t *channel )
{
  int save = 0;
  htsmsg_t *attr;//, *tags;
  epg_episode_t *episode;
  epg_broadcast_t *broadcast;
  const char *id, *start, *stop;
  time_t tm_start, tm_stop;

  if ( data == NULL || channel == NULL ) return 0;

  if ((attr    = htsmsg_get_map(data, "attrib")) == NULL) return 0;
  if ((id      = htsmsg_get_str(attr, "episode")) == NULL) return 0;
  if ((start   = htsmsg_get_str(attr, "start")) == NULL ) return 0;
  if ((stop    = htsmsg_get_str(attr, "stop")) == NULL ) return 0;

  /* Find episode */
  if ((episode = epg_episode_find_by_id(id, 1)) == NULL) return 0;

  /* Parse times */
  if (!_pyepg_parse_time(start, &tm_start)) return 0;
  if (!_pyepg_parse_time(stop, &tm_stop)) return 0;

  /* Find broadcast */
  // TODO: need to think about this
  if ((broadcast = epg_broadcast_find(channel, episode, tm_start, tm_stop, 1)) == NULL) return 0;
  save = 1;

  /* TODO: extra metadata */
  
  return save;
}

int _pyepg_parse_schedule ( htsmsg_t *data )
{
  int save = 0;
  htsmsg_t *attr, *tags;
  htsmsg_field_t *f;
  epg_channel_t *channel;
  const char *str;

  if ( data == NULL ) return 0;

  if ((attr    = htsmsg_get_map(data, "attrib")) == NULL) return 0;
  if ((str     = htsmsg_get_str(attr, "channel")) == NULL) return 0;
  if ((channel = epg_channel_find_by_id(str, 0)) == NULL) return 0;
  if ((tags    = htsmsg_get_map(data, "tags")) == NULL) return 0;

  HTSMSG_FOREACH(f, tags) {
    if (strcmp(f->hmf_name, "broadcast")) {
      save |= _pyepg_parse_broadcast(htsmsg_get_map_by_field(f), channel);
    }
  }

  return save;
}

void _pyepg_parse_epg ( htsmsg_t *data )
{
  int save = 0;
  htsmsg_t *tags;
  htsmsg_field_t *f;

  if ((tags = htsmsg_get_map(data, "tags")) == NULL) return;

  HTSMSG_FOREACH(f, tags) {
    if (strcmp(f->hmf_name, "channel") == 0 ) {
      save |= _pyepg_parse_channel(htsmsg_get_map_by_field(f));
    } else if (strcmp(f->hmf_name, "brand") == 0 ) {
      save |= _pyepg_parse_brand(htsmsg_get_map_by_field(f));
    } else if (strcmp(f->hmf_name, "series") == 0 ) {
      save |= _pyepg_parse_season(htsmsg_get_map_by_field(f));
    } else if (strcmp(f->hmf_name, "episode") == 0 ) {
      save |= _pyepg_parse_episode(htsmsg_get_map_by_field(f));
    } else if (strcmp(f->hmf_name, "schedule") == 0 ) {
      save |= _pyepg_parse_schedule(htsmsg_get_map_by_field(f));
    }
  }

  /* Updated */
  if (save) epg_updated();
}