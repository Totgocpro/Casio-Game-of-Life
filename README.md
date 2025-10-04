# Cobways Game of live simulator

This add-in is a simulator of the game of live optimised for the calculator Graph 90+E (French name).

It is not perfect but it has the main necessary tools :

- Infinite world (max 2500 alive cells before a lot of lag or crash)
- Save (buggy) and load world
- Optimisation focused (for the simulation)
- Change speed of the simulation
- Dark - Light mode
- Edit world
- Pause simulation
- Spawn soup - structures (hard-coded) (rle load function work)
- Other stuff

I'm sorry for the code (It is not perfect), It was my first C program. I will also **not** maintain this project.

I used the fx-sdk and the gint framework.

## Build

To build the project you just need to install the fx-sdk + gint and run this command

`fxsdk build-cg`

And It is done ! you can place the add-in in your calculator, and simulate life !

## Use the add-in

To use the add-in you need to know the keybind :

- [MENU] -> Show the help menu (list of the keybinds) **It is the most important keybind**
- [EXIT] -> The correct way to quit the add-in (or [MENU] [MENU])
- [F1] -> Reset the world
- [F2] -> Save the world in a world slot (there are 4 world slots)
- [F3] -> Load a world
- [F4] -> Generate a random soup (of 100 cells)
- [F5] -> Spawn a Spaceship
- [F6] -> Spawn a Gosper Glider Gun
- [SHIFT] -> Pause or play the simulation
- [ALPHA] -> Edit/Move Mode (Edit allow you to toggle a cell, move the cursor with the Arrow key, use [EXE] to toggle the cell)
- [OPTN] -> Toggle Dark/Light mode
- [+]/[-] -> Zoom in/out
- [x]/[DIV] -> Increase/Decrease simulation speed
