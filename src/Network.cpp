#include "Network.hpp"
#include "Input.hpp"
#include "MemoryManagementUnit.hpp"
#include "Display.hpp"
#include "Processor.hpp"
#include "Timer.hpp"
#include "GameBoy.hpp"

void TestPacket(HostGameState hostGameState);

// NOTE: Might want to make the connect logic TCP (gauranteed) and update gamestate logic UDP

/**
 * Serializes a NetworkGameState.
 */
sf::Packet& operator <<(sf::Packet& packet, const NetworkGameState& networkGameState) {
    packet << networkGameState.uniqueId << networkGameState.name << networkGameState.currentMap;
    
    auto& playerPosition = networkGameState.playerPosition;
    packet << playerPosition.yPosition << playerPosition.xPosition 
           << playerPosition.yBlockPosition << playerPosition.xBlockPosition;

    packet << static_cast<unsigned int>(networkGameState.sprites.size());
    for (auto& sprite : networkGameState.sprites) {
        packet << sprite.spriteIndex << sprite.pictureId << sprite.moveStatus << sprite.direction
               << sprite.yDisplacement << sprite.xDisplacement << sprite.yPosition << sprite.xPosition
               << sprite.canMove << sprite.inGrass;    
    }
    
    return packet;
}

/**
 * Deserializes a NetworkGameState.
 */
sf::Packet& operator >>(sf::Packet& packet, NetworkGameState& networkGameState) {
    packet >> networkGameState.uniqueId >> networkGameState.name >> networkGameState.currentMap;
    
    auto& playerPosition = networkGameState.playerPosition;
    packet >> playerPosition.yPosition >> playerPosition.xPosition 
           >> playerPosition.yBlockPosition >> playerPosition.xBlockPosition;

    // TODO: Make sure this part works
    unsigned int spriteCount;
    packet >> spriteCount;
    networkGameState.sprites.resize(spriteCount);
    for (unsigned int index = 0; index < spriteCount; ++index) {
        auto& sprite = networkGameState.sprites[index];
        packet >> sprite.spriteIndex >> sprite.pictureId >> sprite.moveStatus >> sprite.direction
               >> sprite.yDisplacement >> sprite.xDisplacement >> sprite.yPosition >> sprite.xPosition
               >> sprite.canMove >> sprite.inGrass;    
    }
    
    return packet;
}

/**
 * Serializes a HostGameState.
 */
sf::Packet& operator <<(sf::Packet& packet, const HostGameState& hostGameState) {
    packet << static_cast<unsigned int>(hostGameState.playerGameStates.size());
    for (auto& playerGameState : hostGameState.playerGameStates) {
        auto& playerPosition = playerGameState.playerPosition;
        packet << playerGameState.uniqueId << playerGameState.name << playerGameState.currentMap
               << playerPosition.yPosition << playerPosition.xPosition
               << playerPosition.yBlockPosition << playerPosition.xBlockPosition;
    }

    packet << static_cast<unsigned int>(hostGameState.sprites.size());
    for (auto& sprite : hostGameState.sprites) {
        packet << sprite.spriteIndex << sprite.pictureId << sprite.moveStatus << sprite.direction
               << sprite.yDisplacement << sprite.xDisplacement << sprite.yPosition << sprite.xPosition
               << sprite.canMove << sprite.inGrass;    
    }
    
    return packet;
}

/**
 * Deserializes a HostGameState.
 */
sf::Packet& operator >>(sf::Packet& packet, HostGameState& hostGameState) {
    unsigned int playerCount;
    packet >> playerCount;
    hostGameState.playerGameStates.resize(playerCount);
    for (unsigned int index = 0; index < playerCount; ++index) {
        auto& playerGameState = hostGameState.playerGameStates[index];
        auto& playerPosition = playerGameState.playerPosition;
        packet >> playerGameState.uniqueId >> playerGameState.name >> playerGameState.currentMap
               >> playerPosition.yPosition >> playerPosition.xPosition 
               >> playerPosition.yBlockPosition >> playerPosition.xBlockPosition; 
    }

    unsigned int spriteCount;
    packet >> spriteCount;
    hostGameState.sprites.resize(spriteCount);
    for (unsigned int index = 0; index < spriteCount; ++index) {
        auto& sprite = hostGameState.sprites[index];
        packet >> sprite.spriteIndex >> sprite.pictureId >> sprite.moveStatus >> sprite.direction
               >> sprite.yDisplacement >> sprite.xDisplacement >> sprite.yPosition >> sprite.xPosition
               >> sprite.canMove >> sprite.inGrass;    
    }
    
    return packet;
}

/**
 * Serializes a Connect Request.
 */
sf::Packet& operator <<(sf::Packet& packet, const ConnectRequest& connectRequest) {
    packet << connectRequest.name;
    return packet;
}

/**
 * Deserializes a Connect Request.
 */
sf::Packet& operator >>(sf::Packet& packet, ConnectRequest& connectRequest) {
    packet >> connectRequest.name;
    return packet;
}

/**
 * Serializes a Connect Response.
 */
sf::Packet& operator <<(sf::Packet& packet, const ConnectResponse& connectResponse) {
    packet << connectResponse.uniqueId;
    return packet;
}

/**
 * Deserializes a Connect Response.
 */
sf::Packet& operator >>(sf::Packet& packet, ConnectResponse& connectResponse) {
    packet >> connectResponse.uniqueId;
    return packet;
}

Network::Network() {
    networkMode = NetworkMode::IDLE;
}

void Network::Initialize(MemoryManagementUnit* mmu_, Display* display_, 
				       Timer* timer_, Processor* cpu_, GameBoy* gameboy_, sf::RenderWindow* window_) {
    mmu = mmu_;
	display = display_;
	timer = timer_;
	cpu = cpu_;
    window = window_;
}

/**
 * Prepares socket to asynchronously listen on the specified port.
 */
bool Network::SetupSocket(unsigned short port) {
    if (socket.bind(port) != sf::Socket::Done) {
        return false;
    }
    socket.setBlocking(false);
    
    return true;
}


/**
 * Returns true after successfully acting as host and listening on the specified port.
 */
bool Network::Host(unsigned short port, const std::string& name) {
    this->name = name;
    isHost = true;
    std::cout << "Attempting to host game." << std::endl;
    networkMode = NetworkMode::CONNECTING;
    if (!SetupSocket(port)) {
        std::cout << "Failed to bind to socket." << std::endl;
        networkMode = NetworkMode::FAILED_CONNECTING;
        return false;
    }
    
    std::cout << "Connected as host on port:" << port << std::endl;
    networkMode = NetworkMode::CONNECTED_AS_HOST;
    std::srand(std::time(0)); // use current time as seed for random generator
    uniqueId = std::rand();
    std::cout << "Host uniqueId: " << uniqueId << std::endl;
    return true;
}


/**
 * Returns true after successfully receiving a response from the host.
 */
bool Network::Connect(sf::IpAddress address, unsigned short hostPort, unsigned short port, std::string name) {
    this->name = name;
    isHost = false;
    
    std::cout << "Attempting to connect to host while listening on port: " << port << std::endl;
    networkMode = NetworkMode::CONNECTING;
    if (socket.bind(port) != sf::Socket::Done)
    {
        std::cout << "Failed to bind to socket." << std::endl;
        networkMode = NetworkMode::FAILED_CONNECTING;
        return false;
    }
    
    // Setup request packet
    std::cout << "Setting up request packet to send to port: " << hostPort << std::endl;
    ConnectRequest connectRequest;
    connectRequest.name = name;
    sf::Packet connectRequestPacket;
    connectRequestPacket << static_cast<int>(PacketType::CONNECT_REQUEST) << connectRequest;
    
    // Send a request to connect to host
    if (socket.send(connectRequestPacket, address, hostPort) != sf::Socket::Done) {
        std::cout << "Failed to send connect request." << std::endl;
        networkMode = NetworkMode::FAILED_CONNECTING;
        return false;
    }
        
    // Wait for a response from the host
    std::cout << "Packet sent, waiting for response." << std::endl;
    sf::Packet connectResponsePacket;
    int packetType;
    while (true) {
        if (socket.receive(connectResponsePacket, address, port) != sf::Socket::Done) {
            std::cout << "Failed to receive packet." << std::endl;
            networkMode = NetworkMode::FAILED_CONNECTING;
            return false;
        }
        connectResponsePacket >> packetType;
        
        // Keep looping through until we receive a Connection Response
        // TODO: Make this asynchronous, otherwise we could be waiting forever on a dropped packet
        if (packetType == static_cast<int>(PacketType::CONNECT_RESPONSE)) break;
        std::cout << "Packet type was not CONNECT_RESPONSE, attempting to receive another packet: " << packetType << std::endl;
    }
    
    connectResponsePacket >> uniqueId;
    std::cout << "Connection to host successful with unique id: " << uniqueId << std::endl;
    networkMode = NetworkMode::CONNECTED_AS_CLIENT;
    socket.setBlocking(false);
    
    // Store host's NetworkId
    NetworkId hostNetworkId;
    hostNetworkId.address = address;
    hostNetworkId.port = hostPort;
    clients[0] = hostNetworkId;
    
    return true;
}

/**
 * Handle processing any responses received and requests made. Returns null if
 * no updates to state are received.
 */
HostGameState Network::Update(NetworkGameState& localGameState) {
    //std::cout << "My unique Id: " << uniqueId << std::endl;
    localGameState.uniqueId = uniqueId;
    localGameState.name = name;
    
    if (networkMode == NetworkMode::CONNECTED_AS_HOST) {
        auto host = HostUpdate(localGameState);
        
        for (auto& gs : host.playerGameStates) {
            //std::cout << "Unique Id: " << gs.uniqueId << std::endl;
        }
        
        return host;
        //return HostUpdate(localGameState);
    } else if (networkMode == NetworkMode::CONNECTED_AS_CLIENT) {
        auto host = ClientUpdate(localGameState);
        
        for (auto& gs : host.playerGameStates) {
            //std::cout << "Unique Id: " << gs.uniqueId << std::endl;
        }
        
        return host;
        
        //return ClientUpdate(localGameState);
    } else {
        throw;
    }
}

HostGameState Network::HostUpdate(const NetworkGameState& localGameState) {
    // Loop through all pending packets
    sf::Packet packet;
    sf::IpAddress sender;
    unsigned short port;
    socket.setBlocking(false);
    auto result = sf::Socket::Done;
    while (result == sf::Socket::Done) {
        result = socket.receive(packet, sender, port);
        if (result == sf::Socket::Done) {
            int packetType;
            packet >> packetType;
            if (packetType == static_cast<int>(PacketType::CONNECT_REQUEST)) {
                HandleConnectRequest(packet, sender, port);
            } else if (packetType == static_cast<int>(PacketType::NETWORK_GAME_STATE)) {
                auto gameState = HandleGameStateResponse(packet, sender, port);
                clientGameStates[gameState.uniqueId] = gameState;
                //std::cout << "Received game state from client: " << gameState.uniqueId << std::endl;
            }
        } else if (result == sf::Socket::Disconnected) {
            // TODO Reconnect
        } else if (result == sf::Socket::Error) {
            // Do Nothing
        }
    }
    
    // Send Host Game State to all clients
    HostGameState hostGameState;
    hostGameState.sprites = localGameState.sprites;
    hostGameState.playerGameStates.push_back(localGameState); // Host is the first
    for (const auto& gameState : clientGameStates) {
        hostGameState.playerGameStates.push_back(gameState.second);
    }
    
    //TestPacket(hostGameState);
    
    sf::Packet hostGameStatePacket;
    hostGameStatePacket << static_cast<int>(PacketType::HOST_GAME_STATE) << hostGameState;
    
    int index = 0;
    for (const auto& client : clients) {
        const auto& networkId = client.second;
        socket.send(hostGameStatePacket, networkId.address, networkId.port);
        ++index;
    }
    
    return hostGameState;
}

/**
 * For a connect request from the client, adds them to the client list and responds with their uniqueId.
 */
void Network::HandleConnectRequest(sf::Packet requestPacket, sf::IpAddress sender, unsigned short port) {
    std::cout << "Handling connection request." << std::endl;
    // Respond with a uniqueId and add the requester to the client listen
    ConnectResponse response;
    std::srand(std::time(0)); // use current time as seed for random generator
    response.uniqueId = std::rand();
    
    // Create client Id and add to client list
    NetworkId clientId;
    std::string name;
    requestPacket >> name;
    clientId.uniqueId = response.uniqueId;
    clientId.name = name;
    clientId.address = sender;
    clientId.port = port;
    clients[clientId.uniqueId] = clientId;
    
    // Send response to client with uniqueId
    std::cout << "Sending response to client for uniqueId: " << clientId.uniqueId << std::endl;
    sf::Packet connectResponsePacket;
    connectResponsePacket << static_cast<int>(PacketType::CONNECT_RESPONSE) << response;
    socket.send(connectResponsePacket, sender, port);
    std::cout << "Connection response sent." << std::endl;
}

/**
 * Handle's the client's game state packet and returns the game state they sent here.
 */
NetworkGameState Network::HandleGameStateResponse(sf::Packet gameStatePacket, sf::IpAddress sender, unsigned short port) {
    NetworkGameState clientGameState;
    gameStatePacket >> clientGameState; // TODO: Need to validate this against the IP Address and Port
    return clientGameState;
}

/**
 * Receives 
 */
HostGameState Network::ClientUpdate(const NetworkGameState& localGameState) {
    // Loop through all pending packets
    HostGameState hostGameState;
    sf::Packet packet;
    sf::IpAddress sender;
    unsigned short port;
    auto result = sf::Socket::Done;
    socket.setBlocking(false);
    while (result == sf::Socket::Done) {
        result = socket.receive(packet, sender, port);
        if (result == sf::Socket::Done) {
            int packetType;
            packet >> packetType;
            if (packetType == static_cast<int>(PacketType::CONNECT_RESPONSE)) {
                HandleConnectResponse(packet, sender, port);
            } else if (packetType == static_cast<int>(PacketType::HOST_GAME_STATE)) {
                packet >> hostGameState;
            }
        } else if (result == sf::Socket::Disconnected) {
            // TODO
        } else if (result == sf::Socket::Error) {
            // Do Nothing
        }
    }
    
    // TODO: Send the game state to the host
    sf::Packet localGameStatePacket;
    localGameStatePacket << static_cast<int>(PacketType::NETWORK_GAME_STATE) << localGameState;
    const auto& networkId = clients[0];
    socket.send(localGameStatePacket, networkId.address, networkId.port);
    
    return hostGameState;
}

/**
 * Accepts connect response from host.
 */
void Network::HandleConnectResponse(sf::Packet gameStatePacket, sf::IpAddress sender, unsigned short port) {
    // TODO: Normally this should only be handled during the connect phase, but need to account for this
    // on a re-sent UDP packet for example.
    throw;
}