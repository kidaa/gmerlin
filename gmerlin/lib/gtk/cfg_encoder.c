/*****************************************************************
 
  cfg_encoder.c
 
  Copyright (c) 2003-2004 by Burkhard Plaum - plaum@ipf.uni-stuttgart.de
 
  http://gmerlin.sourceforge.net
 
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
 
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.
 
*****************************************************************/

#include <stdio.h>
#include <string.h>

#include "gtk_dialog.h"
#include <utils.h>
#include <gui_gtk/codecinfo.h>

typedef struct
  {
  GtkWidget * label;
  GtkWidget * combo;
  GList     * strings;
  GtkWidget * config_button;
  GtkWidget * info_button;

  bg_cfg_section_t * cfg_section;
  bg_parameter_func  set_param;
  void * data;
  int selected;
  } encoder_t;

static void get_value(bg_gtk_widget_t * w)
  {
  int i;
  encoder_t * priv;
  priv = (encoder_t*)(w->priv);
  if(!w->value.val_str || (*(w->value.val_str) == '\0'))
    {
    if(w->info->codec_long_names)
      gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(priv->combo)->entry),
                         w->info->codec_long_names[0]);
    else
      gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(priv->combo)->entry),
                         w->info->codec_names[0]);
    return;
    }

  i = 0;

  while(w->info->codec_names[i])
    {
    if(!strcmp(w->value.val_str, w->info->codec_names[i]))
      {
      if(w->info->codec_long_names)
        gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(priv->combo)->entry),
                           w->info->codec_long_names[i]);
      else
        gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(priv->combo)->entry),
                           w->info->codec_names[i]);
      }
    i++;
    }
  }

static void set_value(bg_gtk_widget_t * w)
  {
  encoder_t * priv;
  
  priv = (encoder_t*)(w->priv);
  
  w->value.val_str = bg_strdup(w->value.val_str,
                               w->info->codec_names[priv->selected]);
  }

static void destroy(bg_gtk_widget_t * w)
  {
  encoder_t * priv = (encoder_t*)(w->priv);
  if(w->value.val_str)
    free(w->value.val_str);
  free(priv);
  }

static void attach(void * priv, GtkWidget * table,
                   int * row,
                   int * num_columns)
  {
  GtkWidget * box;
  encoder_t * s = (encoder_t*)priv;
  
  if(*num_columns < 3)
    *num_columns = 3;

  box = gtk_hbox_new(0, 5);
    
  gtk_table_resize(GTK_TABLE(table), *row+1, *num_columns);
  //  gtk_table_attach_defaults(GTK_TABLE(table), b->button,
  //                            0, 1, *row, *row+1);
  gtk_table_attach(GTK_TABLE(table), s->label,
                    0, 1, *row, *row+1, GTK_FILL, GTK_SHRINK, 0, 0);

  gtk_table_attach_defaults(GTK_TABLE(table), s->combo,
                            1, 2, *row, *row+1);

  gtk_box_pack_start_defaults(GTK_BOX(box), s->config_button);
  gtk_box_pack_start_defaults(GTK_BOX(box), s->info_button);
  gtk_widget_show(box);
    
  gtk_table_attach(GTK_TABLE(table), box,
                   2, 3, *row, *row+1, GTK_FILL, GTK_SHRINK, 0, 0);
  
  
  (*row)++;
  }

static gtk_widget_funcs_t funcs =
  {
    get_value: get_value,
    set_value: set_value,
    destroy:   destroy,
    attach:    attach
  };

static GtkWidget * create_pixmap_button(const char * filename)
  {
  GtkWidget * button;
  GtkWidget * image;
  char * path;
  path = bg_search_file_read("icons", filename);
  if(path)
    {
    image = gtk_image_new_from_file(path);
    free(path);
    }
  else
    image = gtk_image_new();

  gtk_widget_show(image);
  button = gtk_button_new();
  gtk_container_add(GTK_CONTAINER(button), image);
  return button;
  }

static void entry_change_callback(GtkWidget * wid, gpointer data)
  {
  const char * codec_name;
  int i;
  bg_gtk_widget_t * w;
  encoder_t * priv;
  
  w = (bg_gtk_widget_t *)data;
  priv = (encoder_t *)(w->priv);

  codec_name = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(priv->combo)->entry));
  
  i = 0;
  
  if(w->info->codec_long_names)
    {
    while(w->info->codec_long_names[i])
      {
      if(!strcmp(codec_name, w->info->codec_long_names[i]))
        {
        priv->selected = i;
        break;
        }
      i++;
      }
    }

  if(w->info->codec_names)
    {
    while(w->info->codec_names[i])
      {
      if(!strcmp(codec_name, w->info->codec_names[i]))
        {
        priv->selected = i;
        break;
        }
      i++;
      }
    }
  if(w->info->codec_parameters[priv->selected])
    gtk_widget_set_sensitive(priv->config_button, 1);
  else
    gtk_widget_set_sensitive(priv->config_button, 0);
  
  }

static void button_callback(GtkWidget * wid, gpointer data)
  {
  bg_gtk_widget_t * w;
  encoder_t * priv;
  bg_dialog_t * dialog;
  
  w = (bg_gtk_widget_t *)data;
  priv = (encoder_t *)(w->priv);

  if(wid == priv->info_button)
    {
    bg_gtk_codec_info_show(w->info, priv->selected);
    }
  else if(wid == priv->config_button)
    {
    dialog = bg_dialog_create(priv->cfg_section, priv->set_param,
                              priv->data,
                              w->info->codec_parameters[priv->selected]);
    bg_dialog_show(dialog);

    }
  }

void bg_gtk_create_encoder(bg_gtk_widget_t * w,
                           bg_parameter_info_t * info,
                           bg_cfg_section_t * cfg_section,
                           bg_parameter_func set_param,
                           void * data)
  {
  int i;
  char * c;
  encoder_t * priv = calloc(1, sizeof(*priv));

  priv->cfg_section = cfg_section;
  priv->set_param   = set_param;
  priv->data        = data;

  w->funcs = &funcs;
  w->priv = priv;
  
  priv->config_button = create_pixmap_button("config_16.png");
  priv->info_button   = create_pixmap_button("info_16.png");

  g_signal_connect(G_OBJECT(priv->config_button), "clicked",
                   G_CALLBACK(button_callback), w);
  g_signal_connect(G_OBJECT(priv->info_button), "clicked",
                   G_CALLBACK(button_callback), w);
  
  gtk_widget_show(priv->config_button);
  gtk_widget_show(priv->info_button);
  
  priv->combo = gtk_combo_new();
  gtk_editable_set_editable(GTK_EDITABLE(GTK_COMBO(priv->combo)->entry), FALSE);

  g_signal_connect(G_OBJECT(GTK_EDITABLE(GTK_COMBO(priv->combo)->entry)),
                   "changed", G_CALLBACK(entry_change_callback),
                   (gpointer)w);

  
  i = 0;
  while(info->codec_names[i])
    {
    if(info->codec_long_names && info->codec_long_names[i])
      c = g_strdup(info->codec_long_names[i]);
    else
      c = g_strdup(info->codec_names[i]);
    priv->strings = g_list_append(priv->strings, c);
    i++;
    }
  gtk_combo_set_popdown_strings(GTK_COMBO(priv->combo), priv->strings);
  gtk_widget_show(priv->combo);

  
  
  priv->label = gtk_label_new(info->long_name);
  gtk_misc_set_alignment(GTK_MISC(priv->label), 0.0, 0.5);

  gtk_widget_show(priv->label);
  }
