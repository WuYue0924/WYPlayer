#include <GL/freeglut.h>

GLuint tex;

void display()
{
    glClearColor(0,0,1,1);
    glClear(GL_COLOR_BUFFER_BIT);

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, tex);

    glBegin(GL_QUADS);
    glTexCoord2f(0,0); glVertex2f(-1, 1);
    glTexCoord2f(0,1); glVertex2f(-1,-1);
    glTexCoord2f(1,1); glVertex2f( 1,-1);
    glTexCoord2f(1,0); glVertex2f( 1, 1);
    glEnd();

    glutSwapBuffers();
}

int main(int argc, char** argv)
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
    glutInitWindowSize(800,600);
    glutCreateWindow("Test");

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    unsigned char data[4] = {255,0,0,255}; // 红色像素
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,data);

    glutDisplayFunc(display);
    glutMainLoop();
    return 0;
}
