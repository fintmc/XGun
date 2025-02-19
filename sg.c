#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Why?????
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glx.h>
#include <X11/Xlib.h>

#define TINY_SEL_TRESHOLD 10

struct vec2 {
  int x, y;
};

struct selection {
  struct vec2 start, end;
};

struct selection selectedArea = {0};

Display* disp = NULL;

Atom deleteAtom = {0};
bool quit = false;
bool LMBdown = false;

void on_event(XEvent e) {
  switch(e.type) {
  case ClientMessage: { // exiting
    if((Atom)e.xclient.data.l[0] == deleteAtom) {
      quit = true;
      return;
    }
  } break;

  case KeyPress:
  case KeyRelease: {
    bool down = (e.type == KeyPress);
    KeySym ks = XLookupKeysym(&e.xkey, 0);
    if(ks == XK_q && !down) {
      quit = true;
      return;
    }
    // ...
  } break;

  case ButtonPress:
  case ButtonRelease: {
    XButtonEvent ev = e.xbutton;
    bool down = (e.type == ButtonPress);
    LMBdown = down && ev.button == Button1;
    if(ev.button == Button1) {
      if(down) {
        selectedArea.start = (struct vec2){ ev.x, ev.y };
        selectedArea.end = (struct vec2){ ev.x, ev.y };
      } else {
        if(abs(selectedArea.start.x - selectedArea.end.x) <= TINY_SEL_TRESHOLD
           && abs(selectedArea.start.y - selectedArea.end.y) <= TINY_SEL_TRESHOLD) {
          // TODO: make sure selection stays w/in the monitor
          selectedArea.start = (struct vec2){
            selectedArea.start.x - 50,
            selectedArea.start.y - 50
          };
          selectedArea.end = (struct vec2){
            selectedArea.end.x + 50,
            selectedArea.end.y + 50
          };
        }
      }
    }
  } break;

  case MotionNotify: {
    XMotionEvent ev = e.xmotion;
    if(LMBdown) {
      selectedArea.end = (struct vec2){ ev.x, ev.y };
    }
  } break;
  default: break;
  }
}

void drain_events() {
  while(XPending(disp) > 0) {
    XEvent e = {0};
    XNextEvent(disp, &e);
    on_event(e);
  }
}

GLuint make_shader(GLenum type, char* path) {
  FILE* f = fopen(path, "r");
  if(!f) {
    printf("ERROR: unable to open shader file\n");
    perror("fopen()");
    return 0;
  }

  fseek(f, 0, SEEK_END);
  size_t size = ftell(f);
  rewind(f);
  char* code = malloc(size+1);
  assert(code && "Go buy more RAM lol");
  size_t num_read = fread(code, 1, size, f);
  code[num_read] = 0;

  GLuint shader = glCreateShader(type);
  glShaderSource(shader, 1, (const char**)&code, NULL);
  glCompileShader(shader);
  int success=1;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
  if(success != GL_TRUE) {
    char backlog[1024] = {0};
    glGetShaderInfoLog(shader, sizeof(backlog), NULL, backlog);
    printf("shader compilation failed:\n%s\n", backlog);
    glDeleteShader(shader);
    shader = 0; // fallthrough
  }
  free(code);
  fclose(f);
  return shader;
}

GLuint make_shaders(char* vert, char* frag) {
  GLuint vs = make_shader(GL_VERTEX_SHADER, vert);
  GLuint fs = make_shader(GL_FRAGMENT_SHADER, frag);

  GLuint program = glCreateProgram();
  glAttachShader(program, vs);
  glAttachShader(program, fs);
  glLinkProgram(program);
  int success; glGetProgramiv(program, GL_LINK_STATUS, &success);
  if(success != GL_TRUE) {
    char buf[1024] = {0};
    glGetProgramInfoLog(program, sizeof(buf), NULL, buf);
    printf("shader program linkage failed:\n%s\n", buf);
    glDeleteProgram(program);
    program = 0; // fallthrough
  }
  glDeleteShader(vs);
  glDeleteShader(fs);
  return program;
}

int main() {
  disp = XOpenDisplay(NULL);
  if(!disp) {
    printf("buy a monitor or something\n");
    return 1;
  }

  int attrs[] = {
    GLX_DEPTH_SIZE, 24,
    GLX_DOUBLEBUFFER,
    GLX_RGBA,
    None
  };
  XVisualInfo* vi = glXChooseVisual(disp, XDefaultScreen(disp), attrs);
  if(!vi) {
    printf("Visual info not found\n");
    return 1;
  }

  printf("Using visual id 0x%lx\n", vi->visualid);

  Window root = XDefaultRootWindow(disp);

  XSetWindowAttributes swa = {0}; // special window parameters
  swa.colormap = XCreateColormap(disp, root, vi->visual, AllocNone);
  swa.event_mask = KeyPressMask | KeyReleaseMask
    | ButtonPressMask | ButtonReleaseMask
    | PointerMotionMask | ExposureMask
    | ClientMessage;
  swa.save_under = True;
  swa.override_redirect = True;

  XWindowAttributes rwa = {0};
  XGetWindowAttributes(disp, root, &rwa);

  Window win = XCreateWindow(disp, root,
    0, 0, rwa.width, rwa.height, 0, vi->depth, InputOutput, vi->visual,
    CWColormap | CWEventMask | CWSaveUnder | CWOverrideRedirect, &swa);

  //XMapWindow(disp, win);

  // handle window closing properly
  deleteAtom = XInternAtom(disp, "WM_DELETE_WINDOW", False);
  XSetWMProtocols(disp, win, &deleteAtom, 1);

  char* name = "SG";
  char* class = "ScreenGun";
  XClassHint hint = {
    .res_class = class,
    .res_name = name
  };
  XSetClassHint(disp, win, &hint);
  XStoreName(disp, win, name);

  // -- OpenGL --

  GLXContext ctx = glXCreateContext(disp, vi, NULL, True);
  glXMakeCurrent(disp, win, ctx);

  printf("OpenGL %s\n", glGetString(GL_VERSION));

  XImage* screenImage = XGetImage(disp, root, 0, 0, rwa.width, rwa.height, AllPlanes, ZPixmap);
  assert(screenImage && screenImage->data);

  GLuint screenTex;
  glGenTextures(1, &screenTex);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, screenTex);
  glTexImage2D(GL_TEXTURE_2D,
               0,
               GL_RGB,
               screenImage->width,
               screenImage->height,
               0,
               GL_BGRA,
               GL_UNSIGNED_BYTE,
               screenImage->data);
  glGenerateMipmap(GL_TEXTURE_2D);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

  GLuint vao, vbo, ebo;
  glGenVertexArrays(1, &vao);
  glGenBuffers(1, &vbo);
  glGenBuffers(1, &ebo);

  glBindVertexArray(vao);

  float vertices[] = {
    // position      tex coords
       -1, -1,       0, 1,        // top left
        1, -1,       1, 1,        // top right
       -1,  1,       0, 0,        // bot left
        1,  1,       1, 0         // bot right
  };

  /*unsigned int indices[] = {
    0, 1, 2,
    1, 2, 3
  };*/

  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

  //glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
  //glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

  glBindVertexArray(vao);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)0);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)(2*sizeof(float)));

  GLuint shader = make_shaders("./sg.vert", "./sg.frag");
  glUseProgram(shader);
  glUniform1i(glGetUniformLocation(shader, "tex"), 0);

  glUniform2f(glGetUniformLocation(shader, "screenSize"), rwa.width, rwa.height);

  XMapWindow(disp, win);

  while(true) {
    XSetInputFocus(disp, win, RevertToParent, CurrentTime);
    drain_events(disp);

    if(quit) break;

    // -- drawing --
    glUseProgram(shader);
    glUniform2f(glGetUniformLocation(shader, "sel1"), selectedArea.start.x, selectedArea.start.y);
    glUniform2f(glGetUniformLocation(shader, "sel2"), selectedArea.end.x, selectedArea.end.y);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    //glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, screenTex);

    //glDrawElements(GL_TRIANGLES, 12, GL_UNSIGNED_INT, 0);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glXSwapBuffers(disp, win);
  }
 cleanup:
  printf("Goodbye.\n");
  glXMakeCurrent(disp, None, NULL);
  glXDestroyContext(disp, ctx);
  XDestroyWindow(disp, win);
  XCloseDisplay(disp);
  return 0;
}
