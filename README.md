## Keeping up to date

Update repository. Navigate to project folder

```bash
cd <path to folder>
```

Get updates
```bash
git pull
```

Flash board
```bash
idf.py -p <PORT> flash monitor
```

Exit monitor with `ctrl+]`

## Setup

### Downloads

Download the [Espressif-idf installer](https://dl.espressif.com/dl/esp-idf/).

Select universal installer. Use default settings.

### Getting project

Open `ESP-IDF 5.3 CMD` from window search bar.

Choose a location where you want the project to be stored and get the path by right-clicking and selecting properties at the bottom. Copy the location text and execute the following
```bash
cd file-path-here
```
For example
```bash
cd C:\\Users\\DanTan\\projects
```

Then execute the following command to download the project.

```bash
git clone https://github.com/Dan-Tan/alarm_system.git
```

Navigate to the project with 
```bash
cd alarm_system
```

### Build the project.

```bash
idf.py build
```

### Flash and monitor the board with

```bash
idf.py -p <PORT> flash monitor
```
replacing `<PORT>` with the port the board is connected to. For example `COM4`.


