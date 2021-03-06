#include <gui_gtk/timeruler.h>

typedef struct bg_nle_track_widget_s bg_nle_track_widget_t;

bg_nle_track_widget_t * bg_nle_track_widget_create(bg_nle_track_t * track,
                                                   bg_nle_timerange_widget_t * tr,
                                                   bg_nle_time_ruler_t * ruler);

void bg_nle_track_widget_destroy(bg_nle_track_widget_t *);

GtkWidget * bg_nle_track_widget_get_panel(bg_nle_track_widget_t *);
GtkWidget * bg_nle_track_widget_get_preview(bg_nle_track_widget_t *);

void bg_nle_track_widget_redraw(bg_nle_track_widget_t * w);

void bg_nle_track_widget_update_selection(bg_nle_track_widget_t * w);
void bg_nle_track_widget_update_visible(bg_nle_track_widget_t * w);
void bg_nle_track_widget_update_zoom(bg_nle_track_widget_t * w);
void bg_nle_track_widget_set_flags(bg_nle_track_widget_t * w, int flags);

void bg_nle_track_widget_update_parameters(bg_nle_track_widget_t * w, bg_cfg_section_t * s);
