
pkgdatadir = $(datadir)/gmerlin/icons

icons = \
about_16.png \
add_16.png \
audio_16.png \
bottom_16.png \
camelot_icon.png \
chapter_16.png \
config_16.png \
close_16.png \
copy_16.png \
cut_16.png \
digit_0.png \
digit_1.png \
digit_2.png \
digit_3.png \
digit_4.png \
digit_5.png \
digit_6.png \
digit_7.png \
digit_8.png \
digit_9.png \
digit_colon.png \
digit_dot.png \
digit_minus.png \
display_mode_all.png \
display_mode_all_rem.png \
display_mode_none.png \
display_mode_rem.png \
down_16.png \
drive_16.png \
drive_error_16.png \
drive_running_16.png \
earth_16.png \
eject_16.png \
favourites_16.png \
favourites_closed_16.png \
favourites_open_16.png \
filter_16.png \
find_16.png \
first_16.png \
folder_closed_16.png \
folder_new_16.png \
folder_open_16.png \
fullscreen_16.png \
goto_current_16.png \
hardware_16.png \
help_16.png \
image_16.png \
incoming_closed_16.png \
incoming_open_16.png \
info_16.png \
kbd_icon.png \
last_16.png \
left_16.png \
lock_16.png \
log_16.png \
mixer_icon.png \
mute_16.png \
paste_16.png \
player_icon.png \
plugin_16.png \
plugincfg_icon.png \
quit_16.png \
record_16.png \
recorder_icon.png \
refresh_16.png \
rename_16.png \
repeat_mode_1.png \
repeat_mode_all.png \
repeat_mode_none.png \
right_16.png \
run_16.png \
save_16.png \
snapshot_16.png \
sort_16.png \
state_buffering_1.png \
state_buffering_2.png \
state_buffering_3.png \
state_buffering_4.png \
state_buffering_5.png \
state_changing.png \
state_error.png \
state_mute.png \
state_paused.png \
state_playing.png \
state_seeking.png \
state_stopped.png \
stop_16.png \
tab_close.png \
top_16.png \
tearoff_16.png \
tracks_dnd_32.png \
trash_16.png \
transcoder_icon.png \
tree_root_16.png \
tuner_16.png \
up_16.png \
video_16.png \
visualizer_icon.png \
windowed_16.png \
gmerlin.jpg

pkgdata_DATA = $(icons)

dist_pkgdata_DATA = $(icons)

install-data-hook:
	test -z $(DESTDIR)$(prefix)/share/icons/hicolor/48x48/apps || $(mkdir_p) $(DESTDIR)$(prefix)/share/icons/hicolor/48x48/apps
	ln -sf $(DESTDIR)$(pkgdatadir)/mixer_icon.png $(DESTDIR)$(prefix)/share/icons/hicolor/48x48/apps/gmerlin-alsamixer.png
	ln -sf $(DESTDIR)$(pkgdatadir)/player_icon.png $(DESTDIR)$(prefix)/share/icons/hicolor/48x48/apps/gmerlin-player.png
	ln -sf $(DESTDIR)$(pkgdatadir)/transcoder_icon.png $(DESTDIR)$(prefix)/share/icons/hicolor/48x48/apps/gmerlin-transcoder.png
	ln -sf $(DESTDIR)$(pkgdatadir)/camelot_icon.png $(DESTDIR)$(prefix)/share/icons/hicolor/48x48/apps/gmerlin-camelot.png
	ln -sf $(DESTDIR)$(pkgdatadir)/kbd_icon.png $(DESTDIR)$(prefix)/share/icons/hicolor/48x48/apps/gmerlin-kbd.png
	ln -sf $(DESTDIR)$(pkgdatadir)/plugincfg_icon.png $(DESTDIR)$(prefix)/share/icons/hicolor/48x48/apps/gmerlin-plugincfg.png
	ln -sf $(DESTDIR)$(pkgdatadir)/recorder_icon.png $(DESTDIR)$(prefix)/share/icons/hicolor/48x48/apps/gmerlin-recorder.png
	ln -sf $(DESTDIR)$(pkgdatadir)/visualizer_icon.png $(DESTDIR)$(prefix)/share/icons/hicolor/48x48/apps/gmerlin-visualizer.png

uninstall-local:
	-rm -rf $(DESTDIR)$(prefix)/share/icons/hicolor/48x48/apps/gmerlin-alsamixer.png
	-rm -rf $(DESTDIR)$(prefix)/share/icons/hicolor/48x48/apps/gmerlin-player.png
	-rm -rf $(DESTDIR)$(prefix)/share/icons/hicolor/48x48/apps/gmerlin-transcoder.png
	-rm -rf $(DESTDIR)$(prefix)/share/icons/hicolor/48x48/apps/gmerlin-camelot.png
	-rm -rf $(DESTDIR)$(prefix)/share/icons/hicolor/48x48/apps/gmerlin-kbd.png
	-rm -rf $(DESTDIR)$(prefix)/share/icons/hicolor/48x48/apps/gmerlin-plugincfg.png
	-rm -rf $(DESTDIR)$(prefix)/share/icons/hicolor/48x48/apps/gmerlin-recorder.png
	-rm -rf $(DESTDIR)$(prefix)/share/icons/hicolor/48x48/apps/gmerlin-visualizer.png



