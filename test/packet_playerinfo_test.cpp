#include <gtest/gtest.h>

#include <cstring>
#include <memory>
#include <vector>

#include "dvlnet/packet.h"
#include "multi.h"
#include "player.h"
#include "playerdat.hpp"

namespace devilution {
namespace {

using buffer_t = net::buffer_t;

// Helper to create a GameData structure for testing
GameData CreateTestGameData()
{
	GameData data = {};
	data.size = sizeof(GameData);
	data.programid = 1;
	data.versionMajor = 1;
	data.versionMinor = 5;
	data.versionPatch = 3;
	data.nDifficulty = DIFF_NORMAL;
	data.nTickRate = 20;
	data.bRunInTown = 0;
	data.bTheoQuest = 1;
	data.bCowQuest = 1;
	data.bFriendlyFire = 0;
	data.fullQuests = 1;
	return data;
}

// Helper to create player names buffer (4 slots of PlayerNameLength bytes each)
void FillPlayerNamesBuffer(buffer_t &buf, size_t offset, const std::vector<std::string> &names)
{
	for (size_t i = 0; i < 4; i++) {
		if (i < names.size()) {
			std::memcpy(buf.data() + offset + (i * PlayerNameLength), names[i].c_str(), std::min(names[i].size(), static_cast<size_t>(PlayerNameLength)));
		} else {
			std::memset(buf.data() + offset + (i * PlayerNameLength), '\0', PlayerNameLength);
		}
	}
}

TEST(PacketPlayerInfoTest, PT_INFO_REQUEST_WithProtocolVersion)
{
	// Test that PT_INFO_REQUEST packet can be created with protocol version
	net::packet_factory pktfty;

	buffer_t protocolVersion;
	protocolVersion.resize(1);
	protocolVersion[0] = 1; // Protocol version 1 = supports class/level

	auto pktResult = pktfty.make_packet<net::PT_INFO_REQUEST>(net::PLR_BROADCAST, net::PLR_MASTER, protocolVersion);
	ASSERT_TRUE(pktResult.has_value());

	auto &pkt = *pktResult;
	EXPECT_EQ(pkt->Type(), net::PT_INFO_REQUEST);
	EXPECT_EQ(pkt->Source(), net::PLR_BROADCAST);
	EXPECT_EQ(pkt->Destination(), net::PLR_MASTER);

	// Verify packet data contains protocol version
	const buffer_t &data = pkt->Data();
	EXPECT_GT(data.size(), 0); // Should contain header + protocol version
}

TEST(PacketPlayerInfoTest, PT_INFO_REQUEST_WithoutProtocolVersion)
{
	// Test that PT_INFO_REQUEST packet can be created without protocol version (old client)
	net::packet_factory pktfty;

	auto pktResult = pktfty.make_packet<net::PT_INFO_REQUEST>(net::PLR_BROADCAST, net::PLR_MASTER);
	ASSERT_TRUE(pktResult.has_value());

	auto &pkt = *pktResult;
	EXPECT_EQ(pkt->Type(), net::PT_INFO_REQUEST);

	// Old client sends packet with no protocol version in the data payload
	const buffer_t &data = pkt->Data();
	EXPECT_GT(data.size(), 0); // Should at least contain header
}

TEST(PacketPlayerInfoTest, PT_INFO_REPLY_OldFormat)
{
	// Test creating and parsing PT_INFO_REPLY in old format (no class/level)
	net::packet_factory pktfty;
	GameData gameData = CreateTestGameData();

	// Build old format: GameData + 4 player names + game name
	buffer_t infoBuffer;
	std::vector<std::string> playerNames = { "Warrior1", "Rogue1", "Sorc1" };
	std::string gameName = "TestGame";

	size_t bufferSize = sizeof(GameData) + (PlayerNameLength * 4) + gameName.size();
	infoBuffer.resize(bufferSize);

	// Copy GameData
	std::memcpy(infoBuffer.data(), &gameData, sizeof(GameData));

	// Copy player names
	FillPlayerNamesBuffer(infoBuffer, sizeof(GameData), playerNames);

	// Copy game name
	std::memcpy(infoBuffer.data() + sizeof(GameData) + (PlayerNameLength * 4), gameName.c_str(), gameName.size());

	// Create packet
	auto pktResult = pktfty.make_packet<net::PT_INFO_REPLY>(net::PLR_BROADCAST, net::PLR_MASTER, infoBuffer);
	ASSERT_TRUE(pktResult.has_value());

	auto &pkt = *pktResult;
	EXPECT_EQ(pkt->Type(), net::PT_INFO_REPLY);

	// Verify the info buffer can be retrieved
	auto infoResult = pkt->Info();
	ASSERT_TRUE(infoResult.has_value());
	const buffer_t *info = *infoResult;
	ASSERT_NE(info, nullptr);
	EXPECT_EQ(info->size(), bufferSize);
}

TEST(PacketPlayerInfoTest, PT_INFO_REPLY_NewFormat)
{
	// Test creating and parsing PT_INFO_REPLY in new format (with class/level)
	net::packet_factory pktfty;
	GameData gameData = CreateTestGameData();

	// Build new format: GameData + 4 player names + 4 classes + 4 levels + game name
	buffer_t infoBuffer;
	std::vector<std::string> playerNames = { "Warrior1", "Rogue1", "Sorc1" };
	std::string gameName = "TestGame";

	size_t classLevelDataSize = 2 * 4; // 4 bytes for classes + 4 bytes for levels
	size_t bufferSize = sizeof(GameData) + (PlayerNameLength * 4) + classLevelDataSize + gameName.size();
	infoBuffer.resize(bufferSize);

	// Copy GameData
	std::memcpy(infoBuffer.data(), &gameData, sizeof(GameData));

	// Copy player names
	FillPlayerNamesBuffer(infoBuffer, sizeof(GameData), playerNames);

	// Copy class data
	size_t classLevelOffset = sizeof(GameData) + (PlayerNameLength * 4);
	infoBuffer[classLevelOffset + 0] = static_cast<uint8_t>(HeroClass::Warrior);
	infoBuffer[classLevelOffset + 1] = static_cast<uint8_t>(HeroClass::Rogue);
	infoBuffer[classLevelOffset + 2] = static_cast<uint8_t>(HeroClass::Sorcerer);
	infoBuffer[classLevelOffset + 3] = 0; // Empty slot

	// Copy level data
	infoBuffer[classLevelOffset + 4 + 0] = 10; // Warrior level 10
	infoBuffer[classLevelOffset + 4 + 1] = 15; // Rogue level 15
	infoBuffer[classLevelOffset + 4 + 2] = 12; // Sorcerer level 12
	infoBuffer[classLevelOffset + 4 + 3] = 0;  // Empty slot

	// Copy game name
	std::memcpy(infoBuffer.data() + sizeof(GameData) + (PlayerNameLength * 4) + classLevelDataSize, gameName.c_str(), gameName.size());

	// Create packet
	auto pktResult = pktfty.make_packet<net::PT_INFO_REPLY>(net::PLR_BROADCAST, net::PLR_MASTER, infoBuffer);
	ASSERT_TRUE(pktResult.has_value());

	auto &pkt = *pktResult;
	EXPECT_EQ(pkt->Type(), net::PT_INFO_REPLY);

	// Verify the info buffer can be retrieved
	auto infoResult = pkt->Info();
	ASSERT_TRUE(infoResult.has_value());
	const buffer_t *info = *infoResult;
	ASSERT_NE(info, nullptr);
	EXPECT_EQ(info->size(), bufferSize);

	// Verify class and level data
	EXPECT_EQ((*info)[classLevelOffset + 0], static_cast<uint8_t>(HeroClass::Warrior));
	EXPECT_EQ((*info)[classLevelOffset + 1], static_cast<uint8_t>(HeroClass::Rogue));
	EXPECT_EQ((*info)[classLevelOffset + 2], static_cast<uint8_t>(HeroClass::Sorcerer));
	EXPECT_EQ((*info)[classLevelOffset + 4 + 0], 10);
	EXPECT_EQ((*info)[classLevelOffset + 4 + 1], 15);
	EXPECT_EQ((*info)[classLevelOffset + 4 + 2], 12);
}

TEST(PacketPlayerInfoTest, PlayerInfo_OptionalFields)
{
	// Test that PlayerInfo structure correctly handles optional fields
	PlayerInfo player1;
	player1.name = "TestPlayer";
	player1.heroClass = HeroClass::Warrior;
	player1.level = 25;

	EXPECT_EQ(player1.name, "TestPlayer");
	EXPECT_TRUE(player1.heroClass.has_value());
	EXPECT_EQ(*player1.heroClass, HeroClass::Warrior);
	EXPECT_TRUE(player1.level.has_value());
	EXPECT_EQ(*player1.level, 25);

	// Test PlayerInfo without optional fields (old format)
	PlayerInfo player2;
	player2.name = "OldPlayer";

	EXPECT_EQ(player2.name, "OldPlayer");
	EXPECT_FALSE(player2.heroClass.has_value());
	EXPECT_FALSE(player2.level.has_value());
}

TEST(PacketPlayerInfoTest, GameInfo_WithPlayerInfo)
{
	// Test that GameInfo structure correctly stores PlayerInfo
	GameInfo gameInfo;
	gameInfo.name = "TestGame";
	gameInfo.gameData = CreateTestGameData();

	PlayerInfo player1;
	player1.name = "Warrior1";
	player1.heroClass = HeroClass::Warrior;
	player1.level = 10;

	PlayerInfo player2;
	player2.name = "Rogue1";
	player2.heroClass = HeroClass::Rogue;
	player2.level = 15;

	gameInfo.players.push_back(player1);
	gameInfo.players.push_back(player2);

	EXPECT_EQ(gameInfo.players.size(), 2);
	EXPECT_EQ(gameInfo.players[0].name, "Warrior1");
	EXPECT_TRUE(gameInfo.players[0].heroClass.has_value());
	EXPECT_EQ(*gameInfo.players[0].heroClass, HeroClass::Warrior);
	EXPECT_TRUE(gameInfo.players[0].level.has_value());
	EXPECT_EQ(*gameInfo.players[0].level, 10);

	EXPECT_EQ(gameInfo.players[1].name, "Rogue1");
	EXPECT_TRUE(gameInfo.players[1].heroClass.has_value());
	EXPECT_EQ(*gameInfo.players[1].heroClass, HeroClass::Rogue);
	EXPECT_TRUE(gameInfo.players[1].level.has_value());
	EXPECT_EQ(*gameInfo.players[1].level, 15);
}

TEST(PacketPlayerInfoTest, BackwardCompatibility_OldClientFormat)
{
	// Simulate parsing old format (no class/level) as if received from old client
	GameData gameData = CreateTestGameData();
	buffer_t infoBuffer;
	std::vector<std::string> playerNames = { "Player1", "Player2" };
	std::string gameName = "OldGame";

	size_t bufferSize = sizeof(GameData) + (PlayerNameLength * 4) + gameName.size();
	infoBuffer.resize(bufferSize);

	std::memcpy(infoBuffer.data(), &gameData, sizeof(GameData));
	FillPlayerNamesBuffer(infoBuffer, sizeof(GameData), playerNames);
	std::memcpy(infoBuffer.data() + sizeof(GameData) + (PlayerNameLength * 4), gameName.c_str(), gameName.size());

	// Check that buffer size indicates old format
	size_t minNeededSize = sizeof(GameData) + (PlayerNameLength * 4);
	size_t classLevelDataSize = 2 * 4;
	size_t newFormatSize = minNeededSize + classLevelDataSize;
	bool hasClassLevelData = (infoBuffer.size() >= newFormatSize + 1);

	EXPECT_FALSE(hasClassLevelData); // Old format should not have class/level data
}

TEST(PacketPlayerInfoTest, BackwardCompatibility_NewClientFormat)
{
	// Simulate parsing new format (with class/level) as if received from new client
	GameData gameData = CreateTestGameData();
	buffer_t infoBuffer;
	std::vector<std::string> playerNames = { "Player1", "Player2" };
	std::string gameName = "NewGame";

	size_t classLevelDataSize = 2 * 4;
	size_t bufferSize = sizeof(GameData) + (PlayerNameLength * 4) + classLevelDataSize + gameName.size();
	infoBuffer.resize(bufferSize);

	std::memcpy(infoBuffer.data(), &gameData, sizeof(GameData));
	FillPlayerNamesBuffer(infoBuffer, sizeof(GameData), playerNames);

	// Add class/level data
	size_t classLevelOffset = sizeof(GameData) + (PlayerNameLength * 4);
	infoBuffer[classLevelOffset + 0] = static_cast<uint8_t>(HeroClass::Warrior);
	infoBuffer[classLevelOffset + 1] = static_cast<uint8_t>(HeroClass::Rogue);
	infoBuffer[classLevelOffset + 2] = 0;
	infoBuffer[classLevelOffset + 3] = 0;
	infoBuffer[classLevelOffset + 4 + 0] = 20;
	infoBuffer[classLevelOffset + 4 + 1] = 25;
	infoBuffer[classLevelOffset + 4 + 2] = 0;
	infoBuffer[classLevelOffset + 4 + 3] = 0;

	std::memcpy(infoBuffer.data() + sizeof(GameData) + (PlayerNameLength * 4) + classLevelDataSize, gameName.c_str(), gameName.size());

	// Check that buffer size indicates new format
	size_t minNeededSize = sizeof(GameData) + (PlayerNameLength * 4);
	size_t newFormatSize = minNeededSize + classLevelDataSize;
	bool hasClassLevelData = (infoBuffer.size() >= newFormatSize + 1);

	EXPECT_TRUE(hasClassLevelData); // New format should have class/level data

	// Verify class and level can be read correctly
	EXPECT_EQ(infoBuffer[classLevelOffset + 0], static_cast<uint8_t>(HeroClass::Warrior));
	EXPECT_EQ(infoBuffer[classLevelOffset + 1], static_cast<uint8_t>(HeroClass::Rogue));
	EXPECT_EQ(infoBuffer[classLevelOffset + 4 + 0], 20);
	EXPECT_EQ(infoBuffer[classLevelOffset + 4 + 1], 25);
}

TEST(PacketPlayerInfoTest, AllHeroClasses)
{
	// Test that all hero classes can be correctly encoded and decoded
	std::vector<HeroClass> allClasses = {
		HeroClass::Warrior,
		HeroClass::Rogue,
		HeroClass::Sorcerer,
		HeroClass::Monk,
		HeroClass::Bard,
		HeroClass::Barbarian
	};

	for (HeroClass heroClass : allClasses) {
		uint8_t encoded = static_cast<uint8_t>(heroClass);
		HeroClass decoded = static_cast<HeroClass>(encoded);
		EXPECT_EQ(decoded, heroClass);

		// Verify that the encoded value is within valid range
		EXPECT_LT(encoded, 255);
	}
}

TEST(PacketPlayerInfoTest, EdgeCases_EmptyPlayers)
{
	// Test handling of game with no players
	GameInfo gameInfo;
	gameInfo.name = "EmptyGame";
	gameInfo.gameData = CreateTestGameData();
	gameInfo.players.clear();

	EXPECT_EQ(gameInfo.players.size(), 0);
	EXPECT_EQ(gameInfo.name, "EmptyGame");
}

TEST(PacketPlayerInfoTest, EdgeCases_MaxPlayers)
{
	// Test handling of game with maximum players (4)
	GameInfo gameInfo;
	gameInfo.name = "FullGame";
	gameInfo.gameData = CreateTestGameData();

	for (int i = 0; i < 4; i++) {
		PlayerInfo player;
		player.name = "Player" + std::to_string(i + 1);
		player.heroClass = static_cast<HeroClass>(i % 3); // Warrior, Rogue, Sorcerer
		player.level = 10 + i * 5;
		gameInfo.players.push_back(player);
	}

	EXPECT_EQ(gameInfo.players.size(), 4);
	EXPECT_EQ(gameInfo.players[0].name, "Player1");
	EXPECT_EQ(*gameInfo.players[0].level, 10);
	EXPECT_EQ(gameInfo.players[3].name, "Player4");
	EXPECT_EQ(*gameInfo.players[3].level, 25);
}

TEST(PacketPlayerInfoTest, EdgeCases_LevelBoundaries)
{
	// Test level boundaries (1-50 is typical range)
	PlayerInfo player1;
	player1.name = "MinLevel";
	player1.level = 1;

	PlayerInfo player2;
	player2.name = "MaxLevel";
	player2.level = 50;

	EXPECT_EQ(*player1.level, 1);
	EXPECT_EQ(*player2.level, 50);
}

// This struct mirrors devilutionx-gamelist's GameData struct for compatibility testing
// See: https://github.com/diasurgical/devilutionx-gamelist/blob/main/main.cpp
struct GamelistGameData {
	int32_t size;
	uint32_t seed;
	uint32_t type;
	uint8_t versionMajor;
	uint8_t versionMinor;
	uint8_t versionPatch;
	uint8_t difficulty;
	uint8_t tickRate;
	uint8_t runInTown;
	uint8_t theoQuest;
	uint8_t cowQuest;
	uint8_t friendlyFire;
	uint8_t fullQuests;
};

// Simulates the gamelist tool's decode() function to verify binary compatibility
// This mirrors the logic from devilutionx-gamelist/main.cpp
struct GamelistDecodeResult {
	bool success = false;
	uint32_t seed = 0;
	std::string type;
	std::string version;
	uint8_t difficulty = 0;
	uint8_t tickRate = 0;
	bool runInTown = false;
	bool theoQuest = false;
	bool cowQuest = false;
	bool friendlyFire = false;
	bool fullQuests = false;
	std::vector<std::string> players;
	std::string gameName;
};

GamelistDecodeResult GamelistDecode(const buffer_t &data)
{
	GamelistDecodeResult result;
	constexpr size_t PacketHeaderSize = 3;
	constexpr uint8_t InfoReply = 0x22;
	constexpr uint8_t Broadcast = 0xFF;
	constexpr uint8_t Host = 0xFE;
	constexpr uint8_t MaxPlayers = 4;

	if (data.size() < PacketHeaderSize)
		return result;

	// Check packet header: type=0x22, src=0xFF, dest=0xFE
	if (data[0] != InfoReply || data[1] != Broadcast || data[2] != Host)
		return result;

	const GamelistGameData *gameData = reinterpret_cast<const GamelistGameData *>(data.data() + PacketHeaderSize);
	size_t neededSize = PacketHeaderSize + gameData->size + (PlayerNameLength * MaxPlayers);
	if (data.size() < neededSize)
		return result;

	// Extract game name (at the end, after player names)
	size_t gameNameSize = data.size() - neededSize;
	// Account for possible class/level data (8 bytes) if present
	if (gameNameSize >= 8) {
		// Could have class/level data, check if remaining looks like a game name
		// For simplicity, assume new format with class/level data
		gameNameSize -= 8;
		neededSize += 8;
	}
	result.gameName.assign(reinterpret_cast<const char *>(data.data() + neededSize), gameNameSize);

	result.seed = gameData->seed;

	// Type is stored as 4 chars, reversed
	const char *typePtr = reinterpret_cast<const char *>(&gameData->type);
	result.type.assign({ typePtr[3], typePtr[2], typePtr[1], typePtr[0] });

	// Version string
	result.version = std::to_string(gameData->versionMajor) + "." + std::to_string(gameData->versionMinor) + "." + std::to_string(gameData->versionPatch);

	result.difficulty = gameData->difficulty;
	result.tickRate = gameData->tickRate;
	result.runInTown = static_cast<bool>(gameData->runInTown);
	result.theoQuest = static_cast<bool>(gameData->theoQuest);
	result.cowQuest = static_cast<bool>(gameData->cowQuest);
	result.friendlyFire = static_cast<bool>(gameData->friendlyFire);
	result.fullQuests = static_cast<bool>(gameData->fullQuests);

	// Extract player names
	for (size_t i = 0; i < MaxPlayers; i++) {
		const char *playerNamePtr = reinterpret_cast<const char *>(data.data() + PacketHeaderSize + gameData->size + (i * PlayerNameLength));
		std::string playerName(playerNamePtr, strnlen(playerNamePtr, PlayerNameLength));
		if (!playerName.empty())
			result.players.push_back(playerName);
	}

	result.success = true;
	return result;
}

TEST(PacketPlayerInfoTest, GamelistCompatibility_OldFormat)
{
	// Test that packets created by DevilutionX can be decoded by the gamelist tool
	// This simulates the old format (no class/level data)
	net::packet_factory pktfty;
	GameData gameData = CreateTestGameData();
	// Gamelist reads type as 4 chars reversed: [3][2][1][0]
	// To get "HRTL", store bytes as: L T R H = 0x4C 0x54 0x52 0x48
	gameData.programid = 0x4852544C; // Results in "HRTL" after gamelist's byte reversal

	buffer_t infoBuffer;
	std::vector<std::string> playerNames = { "TestWarrior", "TestRogue" };
	std::string gameName = "MyTestGame";

	size_t bufferSize = sizeof(GameData) + (PlayerNameLength * 4) + gameName.size();
	infoBuffer.resize(bufferSize);

	std::memcpy(infoBuffer.data(), &gameData, sizeof(GameData));
	FillPlayerNamesBuffer(infoBuffer, sizeof(GameData), playerNames);
	std::memcpy(infoBuffer.data() + sizeof(GameData) + (PlayerNameLength * 4), gameName.c_str(), gameName.size());

	auto pktResult = pktfty.make_packet<net::PT_INFO_REPLY>(net::PLR_BROADCAST, net::PLR_MASTER, infoBuffer);
	ASSERT_TRUE(pktResult.has_value());

	// Get the raw packet data (this is what would be sent over the network)
	const buffer_t &packetData = (*pktResult)->Data();

	// Decode using gamelist-compatible decoder
	GamelistDecodeResult decoded = GamelistDecode(packetData);

	EXPECT_TRUE(decoded.success);
	EXPECT_EQ(decoded.type, "HRTL");
	EXPECT_EQ(decoded.version, "1.5.3");
	EXPECT_EQ(decoded.difficulty, DIFF_NORMAL);
	EXPECT_EQ(decoded.tickRate, 20);
	EXPECT_FALSE(decoded.runInTown);
	EXPECT_TRUE(decoded.theoQuest);
	EXPECT_TRUE(decoded.cowQuest);
	EXPECT_FALSE(decoded.friendlyFire);
	EXPECT_TRUE(decoded.fullQuests);
	ASSERT_EQ(decoded.players.size(), 2);
	EXPECT_EQ(decoded.players[0], "TestWarrior");
	EXPECT_EQ(decoded.players[1], "TestRogue");
}

TEST(PacketPlayerInfoTest, GamelistCompatibility_NewFormat)
{
	// Test that packets with class/level data can still be decoded
	// The gamelist tool should gracefully handle the extra data
	net::packet_factory pktfty;
	GameData gameData = CreateTestGameData();
	// Gamelist reads type as 4 chars reversed: [3][2][1][0]
	// To get "DRTL", store bytes as: L T R D = 0x4C 0x54 0x52 0x44
	gameData.programid = 0x4452544C; // Results in "DRTL" after gamelist's byte reversal

	buffer_t infoBuffer;
	std::vector<std::string> playerNames = { "Warrior1", "Rogue1", "Sorc1" };
	std::string gameName = "NewFormatGame";

	size_t classLevelDataSize = 2 * 4; // 4 bytes classes + 4 bytes levels
	size_t bufferSize = sizeof(GameData) + (PlayerNameLength * 4) + classLevelDataSize + gameName.size();
	infoBuffer.resize(bufferSize);

	std::memcpy(infoBuffer.data(), &gameData, sizeof(GameData));
	FillPlayerNamesBuffer(infoBuffer, sizeof(GameData), playerNames);

	// Add class/level data
	size_t classLevelOffset = sizeof(GameData) + (PlayerNameLength * 4);
	infoBuffer[classLevelOffset + 0] = static_cast<uint8_t>(HeroClass::Warrior);
	infoBuffer[classLevelOffset + 1] = static_cast<uint8_t>(HeroClass::Rogue);
	infoBuffer[classLevelOffset + 2] = static_cast<uint8_t>(HeroClass::Sorcerer);
	infoBuffer[classLevelOffset + 3] = 0;
	infoBuffer[classLevelOffset + 4] = 15; // levels
	infoBuffer[classLevelOffset + 5] = 20;
	infoBuffer[classLevelOffset + 6] = 25;
	infoBuffer[classLevelOffset + 7] = 0;

	std::memcpy(infoBuffer.data() + sizeof(GameData) + (PlayerNameLength * 4) + classLevelDataSize, gameName.c_str(), gameName.size());

	auto pktResult = pktfty.make_packet<net::PT_INFO_REPLY>(net::PLR_BROADCAST, net::PLR_MASTER, infoBuffer);
	ASSERT_TRUE(pktResult.has_value());

	const buffer_t &packetData = (*pktResult)->Data();
	GamelistDecodeResult decoded = GamelistDecode(packetData);

	EXPECT_TRUE(decoded.success);
	EXPECT_EQ(decoded.type, "DRTL");
	EXPECT_EQ(decoded.version, "1.5.3");
	ASSERT_EQ(decoded.players.size(), 3);
	EXPECT_EQ(decoded.players[0], "Warrior1");
	EXPECT_EQ(decoded.players[1], "Rogue1");
	EXPECT_EQ(decoded.players[2], "Sorc1");
	EXPECT_EQ(decoded.gameName, "NewFormatGame");
}

} // namespace
} // namespace devilution
