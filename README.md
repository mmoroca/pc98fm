# pc98fm

A small two-panel file manager inspired by NEC PC-98 software from the late 1980s and early 1990s.

The project is written in portable C17. The terminal interface uses ncurses on POSIX systems and can use PDCurses on Windows.

By [@mmoroca](https://x.com/mmoroca) & arena.ai 2026.

<img src="https://raw.githubusercontent.com/mmoroca/pc98fm/refs/heads/main/pc98fm%20-%20main%20window.png" alt="Main window of the pc98fm file manager, showing a dark retro terminal interface with two side-by-side directory panels, a top status bar, and cyan borders. The left panel lists project folders and files with green, yellow, and orange text for different item types, while the right panel shows file metadata and dates. The interface has a technical, utilitarian mood and includes text such as pc98fm, PATH, FREE, FILE, and various directory and file names." width="45%">   <img src="https://raw.githubusercontent.com/mmoroca/pc98fm/refs/heads/main/pc98fm%20-%20editor%20window.png" alt="Editor window of the pc98fm application, showing a dark terminal-style text editing view with a single file pane, menu text, and a cursor position indicator. The wider environment is a retro desktop interface with a cyan border and command-like text formatting, creating a focused, functional mood. Visible text includes editor controls, file names, and status information such as page labels and time." width="45%">   

## Features

- Two-panel directory navigation.
- Directory sorting by name, extension, size or date.
- `.` and `..` always remain at the top of a panel.
- Text viewer with UTF-8-aware display.
- Line editor with insertion, deletion, horizontal scrolling and Unicode-aware cursor placement.
- Copy, rename, delete and directory creation.
- Recursive directory copy and delete operations.
- Executable detection and foreground execution.
- ZIP packing and unpacking without external `zip`, `unzip` or LHA commands.
- Temporary progress dialogs for potentially slow operations.
- Standard ncurses colours for directories, executables, files and selections.
- Portable filesystem backends for POSIX systems and Windows.

## Building on macOS and Linux

The traditional Makefile requires a C17 compiler and ncurses:

```sh
make
./pc98fm
```

To remove the executable:

```sh
make clean
```

A CMake configuration is also provided:

```sh
cmake -S . -B build
cmake --build build
```

On macOS, install the system Command Line Tools if a compiler or ncurses headers are not already available.

## Windows

The Windows build uses the Win32 filesystem backend and PDCurses. PDCurses must be installed and made available to CMake:

```sh
cmake -S . -B build \
  -DPDCURSES_INCLUDE_DIR=C:/path/to/pdcurses \
  -DPDCURSES_LIBRARY=C:/path/to/pdcurses/pdcurses.a
cmake --build build
```

The current Windows filesystem backend uses ANSI Win32 functions. Full Unicode filename handling on Windows remains a future improvement.

## Keyboard commands

| Key | Action |
|---|---|
| `Tab` | Switch active panel |
| Arrow keys | Move the selection |
| `Page Up`, `Page Down` | Move by a page |
| `Home`, `End` | First or last entry |
| `Enter` | Enter the selected directory |
| `V` | View the selected file |
| `E` | Edit the selected file |
| `X` | Execute the selected executable |
| `C` or `F5` | Copy |
| `R` or `F6` | Rename |
| `N` or `F7` | Create a directory |
| `D` or `F8` | Delete |
| `S` | Select or reverse the sort criterion |
| `P` | Pack the selected item as ZIP |
| `U` | Unpack the selected ZIP archive |
| `:` | Select a drive or mount point |
| `Ctrl-R` | Reload the active panel |
| `Q` or `Esc` | Ask for confirmation and exit |

`Esc` has the context-specific meaning of closing the viewer or editor while either of those modes is active. The editor asks whether modified content should be saved.

## Execution

`X` executes a selected executable in the foreground. The file manager uses the selected panel directory as the child process working directory and resumes after the process exits.

On POSIX systems the implementation uses `fork()`, `exec()` and `waitpid()` rather than invoking a shell. The curses screen is suspended while the child owns the terminal. On Windows it uses `CreateProcess()` and waits for the process to finish.

On POSIX systems, a regular file is shown as executable when one of its execute permission bits is set. On Windows, the initial executable extensions are `.exe`, `.com`, `.bat` and `.cmd`.

## ZIP packing and unpacking

`P` creates an archive in the other panel using the selected name with `.zip` appended.

- Packing a directory preserves the selected directory as the archive's top-level directory.
- Packing a single file stores only that file; no artificial containing directory is added.
- `U` extracts archive entries directly into the other panel.
- Existing top-level extraction targets require overwrite confirmation.
- Absolute paths, `..` components and backslash-based archive paths are rejected during extraction.
- Encrypted and unsupported ZIP entries are rejected.

ZIP support is integrated from the vendored `miniz` sources in `src/third_party/`. No external compression executable or runtime compression library is required. The upstream copyright and licence notices are retained in that directory.

## Colours

The interface uses standard curses colour names rather than changing the terminal palette:

- Directories: cyan.
- Executables: red.
- Ordinary files: yellow.
- Selected entries: the selection highlight takes precedence.

The exact RGB appearance is intentionally left to the terminal emulator so that the same code remains portable across ncurses, macOS terminals, Konsole and PDCurses.

## Project layout

- `src/main.c`: application loop and key dispatch.
- `src/core.c`, `src/core.h`: portable application state and operations.
- `src/fs.h`: common filesystem interface.
- `src/fs_posix.c`: POSIX filesystem backend.
- `src/fs_windows.c`: Windows filesystem backend.
- `src/archive_zip.c`, `src/archive_zip.h`: ZIP operations and extraction safety checks.
- `src/third_party/`: integrated miniz sources and their licence notice.
- `src/ui.h`: terminal UI interface.
- `src/ui_curses.c`: ncurses/PDCurses user interface.

## Licence

The pc98fm project code is released under the MIT Licence. Vendored third-party code retains its own copyright and licence notices; see `src/third_party/miniz_LICENSE.txt`.

---

# pc98fm — Español

Un pequeño gestor de archivos de dos paneles inspirado en el software de NEC PC-98 de finales de los años 80 y principios de los 90.

El proyecto está escrito en C17 portable. La interfaz de terminal utiliza ncurses en sistemas POSIX y puede utilizar PDCurses en Windows.

By [@mmoroca](https://x.com/mmoroca) & arena.ai 2026.

<img src="https://raw.githubusercontent.com/mmoroca/pc98fm/refs/heads/main/pc98fm%20-%20main%20window.png" alt="Ventana principal del administrador de archivos pc98fm, que muestra una interfaz oscura de estilo terminal retro con dos paneles de directorios dispuestos lado a lado, una barra de estado superior y bordes de color cian. El panel izquierdo enumera carpetas y archivos del proyecto con texto en verde, amarillo y naranja según el tipo de elemento, mientras que el panel derecho muestra metadatos y fechas de los archivos. La interfaz posee un carácter técnico y utilitario, e incluye texto como «pc98fm», «PATH», «FREE», «FILE», así como diversos nombres de directorios y archivos." width="45%">   
<img src="https://raw.githubusercontent.com/mmoroca/pc98fm/refs/heads/main/pc98fm%20-%20editor%20window.png" alt="Ventana del editor de la aplicación pc98fm, que muestra una vista de edición de texto oscura al estilo de una terminal, con un único panel de archivos, texto de menú e indicador de posición del cursor. El entorno general presenta una interfaz de escritorio retro con un borde cian y un formato de texto similar al de una línea de comandos, lo que genera una atmósfera funcional y propicia para la concentración. Entre el texto visible se incluyen controles del editor, nombres de archivo e información de estado, como etiquetas de página y la hora." width="45%">   

## Funcionalidades

- Navegación en dos paneles.
- Ordenación por nombre, extensión, tamaño o fecha.
- `.` y `..` permanecen siempre al principio del panel.
- Visor de texto compatible con la visualización UTF-8.
- Editor de líneas con inserción, borrado, desplazamiento horizontal y cursor consciente de Unicode.
- Copiar, renombrar, borrar y crear directorios.
- Copia y borrado recursivos de directorios.
- Detección y ejecución de archivos ejecutables.
- Empaquetado y desempaquetado ZIP sin depender de comandos externos `zip`, `unzip` o LHA.
- Diálogos temporales de progreso para operaciones potencialmente lentas.
- Colores estándar de ncurses para directorios, ejecutables, archivos y selección.
- Backends de filesystem para sistemas POSIX y Windows.

## Compilación en macOS y Linux

El Makefile tradicional necesita un compilador C17 y ncurses:

```sh
make
./pc98fm
```

Para eliminar el ejecutable:

```sh
make clean
```

También se proporciona una configuración de CMake:

```sh
cmake -S . -B build
cmake --build build
```

En macOS, instala las Command Line Tools si todavía no están disponibles el compilador o las cabeceras de ncurses.

## Windows

La compilación de Windows utiliza el backend de filesystem de Win32 y PDCurses. PDCurses debe estar instalado y disponible para CMake:

```sh
cmake -S . -B build \
  -DPDCURSES_INCLUDE_DIR=C:/ruta/a/pdcurses \
  -DPDCURSES_LIBRARY=C:/ruta/a/pdcurses/pdcurses.a
cmake --build build
```

El backend actual de Windows utiliza funciones ANSI de Win32. El soporte completo de nombres de archivo Unicode en Windows queda como mejora futura.

## Comandos de teclado

| Tecla | Acción |
|---|---|
| `Tab` | Cambiar el panel activo |
| Flechas | Mover la selección |
| `Page Up`, `Page Down` | Mover una página |
| `Home`, `End` | Primera o última entrada |
| `Enter` | Entrar en el directorio seleccionado |
| `V` | Ver el archivo seleccionado |
| `E` | Editar el archivo seleccionado |
| `X` | Ejecutar el archivo seleccionado |
| `C` o `F5` | Copiar |
| `R` o `F6` | Renombrar |
| `N` o `F7` | Crear un directorio |
| `D` o `F8` | Borrar |
| `S` | Elegir o invertir el criterio de ordenación |
| `P` | Empaquetar el elemento seleccionado como ZIP |
| `U` | Desempaquetar el ZIP seleccionado |
| `:` | Seleccionar una unidad o punto de montaje |
| `Ctrl-R` | Recargar el panel activo |
| `Q` o `Esc` | Pedir confirmación y salir |

`Esc` tiene el significado específico de cerrar el visor o el editor cuando uno de esos modos está activo. El editor pregunta si se deben guardar los cambios pendientes.

## Ejecución

`X` ejecuta un archivo ejecutable en primer plano. El gestor utiliza como directorio de trabajo del proceso el directorio del panel activo y recupera el control cuando el proceso termina.

En sistemas POSIX se utilizan `fork()`, `exec()` y `waitpid()` en lugar de invocar una shell. La pantalla curses se suspende mientras el proceso hijo utiliza la terminal. En Windows se utiliza `CreateProcess()` y se espera a que termine el proceso.

En sistemas POSIX, un archivo normal se muestra como ejecutable cuando tiene activado alguno de sus bits de ejecución. En Windows, inicialmente se reconocen las extensiones `.exe`, `.com`, `.bat` y `.cmd`.

## Empaquetado y desempaquetado ZIP

`P` crea un archivo en el otro panel añadiendo `.zip` al nombre del elemento seleccionado.

- Al empaquetar un directorio se conserva ese directorio como directorio raíz del archivo.
- Al empaquetar un archivo individual solo se guarda ese archivo; no se añade un directorio contenedor artificial.
- `U` extrae las entradas del archivo directamente en el otro panel.
- Los destinos de primer nivel existentes requieren confirmación antes de reemplazarse.
- Durante la extracción se rechazan rutas absolutas, componentes `..` y rutas con separadores invertidos.
- Se rechazan entradas ZIP cifradas o con métodos no soportados.

El soporte ZIP está integrado mediante los fuentes de `miniz` incluidos en `src/third_party/`. No se necesita ningún ejecutable externo ni biblioteca de compresión instalada en tiempo de ejecución. Los avisos de copyright y licencia originales se conservan en ese directorio.

## Colores

La interfaz utiliza nombres de color estándar de curses sin modificar la paleta de la terminal:

- Directorios: cian.
- Ejecutables: rojo.
- Archivos normales: amarillo.
- Elementos seleccionados: el resaltado de selección tiene prioridad.

El aspecto RGB exacto queda intencionadamente en manos del emulador de terminal para mantener la portabilidad entre ncurses, terminales de macOS, Konsole y PDCurses.

## Estructura del proyecto

- `src/main.c`: bucle principal y distribución de teclas.
- `src/core.c`, `src/core.h`: estado portable y operaciones de la aplicación.
- `src/fs.h`: interfaz común del filesystem.
- `src/fs_posix.c`: backend de filesystem POSIX.
- `src/fs_windows.c`: backend de filesystem de Windows.
- `src/archive_zip.c`, `src/archive_zip.h`: operaciones ZIP y comprobaciones de seguridad de extracción.
- `src/third_party/`: fuentes integradas de miniz y su aviso de licencia.
- `src/ui.h`: interfaz de la interfaz de terminal.
- `src/ui_curses.c`: interfaz ncurses/PDCurses.

## Licencia

El código propio del proyecto pc98fm se distribuye bajo la Licencia MIT. El código de terceros incluido conserva sus propios avisos de copyright y licencia; consulta `src/third_party/miniz_LICENSE.txt`.
