/*****************************************************************
 
  searchpath.c
 
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
#include <stdlib.h>
#include <errno.h>

#include <sys/stat.h>
#include <sys/types.h>


#include <limits.h>

#include <config.h>
#include <utils.h>

char * bg_search_file_read(const char * directory, const char * file)
  {
  char * testpath;
  char * home_dir;
  FILE * testfile;

  if(!file)
    return (char*)0;
  
  testpath = malloc(PATH_MAX * sizeof(char));
  
  /* First step: Try home directory */
  
  home_dir = getenv("HOME");
  if(home_dir)
    {
    sprintf(testpath, "%s/.%s/%s/%s", home_dir,
            PACKAGE, directory, file);
    /*    fprintf(stderr, "Trying %s\n", testpath); */
    testfile = fopen(testpath, "r");
    if(testfile)
      {
      fclose(testfile);
      return testpath;
      }
    }
  /* Second step: Try Data directory */
  sprintf(testpath, "%s/%s/%s", GMERLIN_DATA_DIR, directory, file);
  /*  fprintf(stderr, "Trying %s\n", testpath); */
  testfile = fopen(testpath, "r");
  if(testfile)
    {
    fclose(testfile);
    return testpath;
    }
  free(testpath);
  return (char*)0;
  }

char * bg_search_file_write(const char * directory, const char * file)
  {
  char * home_dir;
  char * testpath;
  char * pos1;
  char * pos2;
  FILE * testfile;

  if(!file)
    return (char*)0;
  
  testpath = malloc(PATH_MAX * sizeof(char));
  
  home_dir = getenv("HOME");

  /* Try to open the file */

  sprintf(testpath, "%s/.%s/%s/%s", home_dir, PACKAGE, directory, file);
  testfile = fopen(testpath, "a");
  if(testfile)
    {
    fclose(testfile);
    return testpath;
    }
  
  if(errno != ENOENT)
    {
    free(testpath);
    return (char*)0;
    }

  /*
   *  No such file or directory can mean, that the directory 
   *  doesn't exist
   */
  
  pos1 = &(testpath[strlen(home_dir)+1]);
  
  while(1)
    {
    pos2 = strchr(pos1, '/');

    if(!pos2)
      break;

    *pos2 = '\0';

    if(mkdir(testpath, S_IRUSR|S_IWUSR|S_IXUSR) == -1)
      {
      if(errno != EEXIST)
        {
        *pos2 = '/';
        break;
        }
      }
    else
      fprintf(stderr, "Created directory %s\n", testpath);
    
    *pos2 = '/';
    pos1 = pos2;
    pos1++;
    }

  /* Try once more to open the file */

  testfile = fopen(testpath, "w");

  if(testfile)
    {
    fclose(testfile);
    return testpath;
    }
  free(testpath);
  return (char*)0;
  }

