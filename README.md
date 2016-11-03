# bare-bones-socket-chat

Using bare C sockets to create simple chat room

- Version 1

    Uses one server and one client. This is single threaded and only allows for one client to connect to to the server at a single time.

- Version 2

    Users multithreading to allow one server to handle multiple clients. Up to the specified max number of clients that are allowed.


##Requirements

- For MacOSX: g++ >=4.9 must be installed for all functionallity to work. the cmake build step will try and use g++-4.9 for all compilation

- cmake

##Run

1. Create a build folder in the main directory and cd into it

        mkdir _build; cd _build

2. Generate the build files with the cmake command

        cmake ..

3. Compile all files

        make

4. Run the executable you would like

    - for server version 1

            ./server

    - for client version 1

            ./client 0.0.0.0

    - for server version 2

            ./server2

    - for client version 2

            ./client2 0.0.0.0
