/*****************************************************************
 
  message.h
 
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

/*
 * Pop up a dialog with "message", and an ok 
 * button
 */

#define BG_GTK_MESSAGE_INFO  0
#define BG_GTK_MESSAGE_ERROR 1

void bg_gtk_message(const char * question, int type, GtkWidget * parent);
