#include <string>
#include <queue>
#include <fstream>
#include "Image.hpp"
#include "Label.hpp"
#include "ImageButton.hpp"
#include "PlayScene.hpp"
#include "LOG.hpp"
#include "Enemy.hpp"
#include "PlaneEnemy.hpp"
#include "SoldierEnemy.hpp"
#include "TankEnemy.hpp"
#include "TankEnemy_model2.hpp"
#include "MachineGunTurret.hpp"
#include "MachineGunTurret_model2.hpp"
#include "LaserTurret.hpp"
#include "MissileTurret.hpp"
#include "TurretButton.hpp"
#include "Plane.hpp"
#include "DirtyEffect.hpp"
#include "AudioHelper.hpp"

bool PlayScene::DebugMode = false;
const std::vector<Engine::Point> PlayScene::directions = { Engine::Point(-1, 0), Engine::Point(0, -1), Engine::Point(1, 0), Engine::Point(0, 1) };
const int PlayScene::MapWidth = 20, PlayScene::MapHeight = 13;
const int PlayScene::BlockSize = 64;
const Engine::Point PlayScene::SpawnGridPoint = Engine::Point(-1, 0);
const Engine::Point PlayScene::EndGridPoint = Engine::Point(MapWidth, MapHeight - 1);
const std::vector<int> PlayScene::code = { ALLEGRO_KEY_UP, ALLEGRO_KEY_UP, ALLEGRO_KEY_DOWN, ALLEGRO_KEY_DOWN,
									ALLEGRO_KEY_LEFT, ALLEGRO_KEY_RIGHT, ALLEGRO_KEY_LEFT, ALLEGRO_KEY_RIGHT,
									ALLEGRO_KEY_B, ALLEGRO_KEY_A, ALLEGRO_KEYMOD_SHIFT, ALLEGRO_KEY_ENTER };
Engine::Point PlayScene::GetClientSize() {
	return Engine::Point(MapWidth * BlockSize, MapHeight * BlockSize);
}
void PlayScene::Initialize() {
	mapState.clear();
	keyStrokes.clear();
	ticks = 0;
	lives = 10;
	money = 150;
	SpeedMult = 1;
	// Add groups from bottom to top.
	AddNewObject(TileMapGroup = new Group());
	AddNewObject(GroundEffectGroup = new Group());
	AddNewObject(DebugIndicatorGroup = new Group());
	AddNewObject(TowerGroup = new Group());
	AddNewObject(EnemyGroup = new Group());
	AddNewObject(BulletGroup = new Group());
	AddNewObject(EffectGroup = new Group());
	// Should support buttons.
	AddNewControlObject(UIGroup = new Group());
	ReadMap();
	ReadEnemyWave();
	mapDistance = CalculateBFSDistance();
	ConstructUI();
	imgTarget = new Engine::Image("play/target.png", 0, 0);
	imgTarget->Visible = false;
	UIGroup->AddNewObject(imgTarget);
	AudioHelper::PlayBGM("play.ogg");
}
void PlayScene::Update(float deltaTime) {
	// Fake speed up.
	deltaTime *= SpeedMult;
	// If we use deltaTime directly, then we might have Bullet-through-paper problem.
	// Reference: Bullet-Through-Paper
	if (deltaTime < 3) {
		IScene::Update(deltaTime);
	} else if (deltaTime < 6) {
		IScene::Update(deltaTime / 2);
		IScene::Update(deltaTime / 2);
	} else if (deltaTime < 9) {
		IScene::Update(deltaTime / 3);
		IScene::Update(deltaTime / 3);
		IScene::Update(deltaTime / 3);
	} else {
		IScene::Update(deltaTime / 4);
		IScene::Update(deltaTime / 4);
		IScene::Update(deltaTime / 4);
		IScene::Update(deltaTime / 4);
	}
	if (preview) {
		preview->Position = Engine::GameEngine::GetInstance().GetMousePosition();
	}
	// Check if we should create new enemy.
	ticks += deltaTime;
	if (enemyWaveData.empty()) {
		if (EnemyGroup->GetObjects().empty()) {
			// Free resources.
			/*delete TileMapGroup;
			delete GroundEffectGroup;
			delete DebugIndicatorGroup;
			delete TowerGroup;
			delete EnemyGroup;
			delete BulletGroup;
			delete EffectGroup;
			delete UIGroup;
			delete imgTarget;*/
			// Win.
			Engine::GameEngine::GetInstance().ChangeScene("win");
		}
		return;
	}
	auto current = enemyWaveData.front();
	if (ticks < current.second)
		return;
	ticks -= current.second;
	enemyWaveData.pop_front();
	const Engine::Point SpawnCoordinate = Engine::Point(SpawnGridPoint.x * BlockSize + BlockSize / 2, SpawnGridPoint.y * BlockSize + BlockSize / 2);
	Enemy* enemy;
	switch (current.first) {
	case 1:
		EnemyGroup->AddNewObject(enemy = new SoldierEnemy(SpawnCoordinate.x, SpawnCoordinate.y));
		break;
	case 2:
		EnemyGroup->AddNewObject(enemy = new PlaneEnemy(SpawnCoordinate.x, SpawnCoordinate.y));
		break;
	case 3:
		EnemyGroup->AddNewObject(enemy = new TankEnemy(SpawnCoordinate.x, SpawnCoordinate.y));
		break;
	case 4:
		EnemyGroup->AddNewObject(enemy = new TankEnemy_model2(SpawnCoordinate.x, SpawnCoordinate.y));
		break;	
	default:
		return;
	}
	enemy->UpdatePath(mapDistance);
	// Compensate the time lost.
	enemy->Update(ticks);
}
void PlayScene::Draw() const {
	IScene::Draw();
	if (DebugMode) {
		// Draw reverse BFS distance on all reachable blocks.
		for (int i = 0; i < MapHeight; i++) {
			for (int j = 0; j < MapWidth; j++) {
				if (mapDistance[i][j] != -1) {
					// Not elegant nor efficient, but it's quite enough for debugging.
					Engine::Label label(std::to_string(mapDistance[i][j]), "pirulen.ttf", 32, (j + 0.5) * BlockSize, (i + 0.5) * BlockSize);
					label.Anchor = Engine::Point(0.5, 0.5);
					label.Draw();
				}
			}
		}
	}
}
void PlayScene::OnMouseDown(int button, int mx, int my) {
	if ((button & 1) && !imgTarget->Visible && preview) {
		// Cancel turret construct.
		UIGroup->RemoveObject(preview->GetObjectIterator());
		preview = nullptr;
	}
	IScene::OnMouseDown(button, mx, my);
}
void PlayScene::OnMouseMove(int mx, int my) {
	IScene::OnMouseMove(mx, my);
	const int x = mx / BlockSize;
	const int y = my / BlockSize;
	if (!preview || x < 0 || x >= MapWidth || y < 0 || y >= MapHeight) {
		imgTarget->Visible = false;
		return;
	}
	imgTarget->Visible = true;
	imgTarget->Position.x = x * BlockSize;
	imgTarget->Position.y = y * BlockSize;
}
void PlayScene::OnMouseUp(int button, int mx, int my) {
	IScene::OnMouseUp(button, mx, my);
	if (!imgTarget->Visible)
		return;
	const int x = mx / BlockSize;
	const int y = my / BlockSize;
	if (button & 1) {
		if (mapState[y][x] != TILE_OCCUPIED) {
			if (!preview)
				return;
			// Check if valid.
			if (!CheckSpaceValid(x, y)) {
				Engine::Sprite* sprite;
				GroundEffectGroup->AddNewObject(sprite = new DirtyEffect("play/target-invalid.png", 1, x * BlockSize + BlockSize / 2, y * BlockSize + BlockSize / 2));
				sprite->Rotation = 0;
				return;
			}
			// Purchase.
			EarnMoney(-preview->GetPrice());
			// Remove Preview.
			preview->GetObjectIterator()->first = false;
			UIGroup->RemoveObject(preview->GetObjectIterator());
			// Construct real turret.
			preview->Position.x = x * BlockSize + BlockSize / 2;
			preview->Position.y = y * BlockSize + BlockSize / 2;
			preview->Enabled = true;
			preview->Preview = false;
			preview->Tint = al_map_rgba(255, 255, 255, 255);
			TowerGroup->AddNewObject(preview);
			// Remove Preview.
			preview = nullptr;

			mapState[y][x] = TILE_OCCUPIED;
			OnMouseMove(mx, my);
		}
	}
}
void PlayScene::OnKeyDown(int keyCode) {
	IScene::OnKeyDown(keyCode);
	if (keyCode == ALLEGRO_KEY_TAB) {
		DebugMode = !DebugMode;
	}
	else {
		keyStrokes.push_back(keyCode);
		if (keyStrokes.size() > code.size())
			keyStrokes.pop_front();
		if (keyCode == ALLEGRO_KEY_ENTER && keyStrokes.size() == code.size()) {
			auto it = keyStrokes.begin();
			for (int c : code) {
				if (*it != c && (c == ALLEGRO_KEYMOD_SHIFT && *it != ALLEGRO_KEY_LSHIFT && *it != ALLEGRO_KEY_RSHIFT))
					return;
				++it;
			}
			EffectGroup->AddNewObject(new Plane());
			money += 10000;
		}
	}
	if (keyCode == ALLEGRO_KEY_Q) {
		// Hotkey for MachineGunTurret.
		UIBtnClicked(0);
	}
	else if (keyCode == ALLEGRO_KEY_W) {
		// Hotkey for LaserTurret.
		UIBtnClicked(1);
	}
	else if (keyCode == ALLEGRO_KEY_E) {
		// Hotkey for MissileTurret.
		UIBtnClicked(2);
	}
	else if (keyCode == ALLEGRO_KEY_R) {
		// Hotkey for MissileTurret.
		UIBtnClicked(3);
	}
	else if (keyCode >= ALLEGRO_KEY_0 && keyCode <= ALLEGRO_KEY_9) {
		// Hotkey for Speed up.
		SpeedMult = keyCode - ALLEGRO_KEY_0;
	}
}
void PlayScene::Hit() {
	lives--;
	UILives->Text = std::string("Life ") + std::to_string(lives);
	if (lives <= 0) {
		Engine::GameEngine::GetInstance().ChangeScene("lose");
	}
}
int PlayScene::GetMoney() const {
	return money;
}
void PlayScene::EarnMoney(int money) {
	this->money += money;
	UIMoney->Text = std::string("$") + std::to_string(this->money);
}
void PlayScene::ReadMap() {
	std::string filename = std::string("resources/map") + std::to_string(MapId) + ".txt";
	// Read map file.
	char c;
	std::vector<bool> mapData;
	std::ifstream fin(filename);
	while (fin >> c) {
		switch (c) {
		case '0': mapData.push_back(false); break;
		case '1': mapData.push_back(true); break;
		case '\n':
		case '\r':
			if (static_cast<int>(mapData.size()) / MapWidth != 0)
				throw std::ios_base::failure("Map data is corrupted.");
			break;
		default: throw std::ios_base::failure("Map data is corrupted.");
		}
	}
	fin.close();
	// Validate map data.
	if (static_cast<int>(mapData.size()) != MapWidth * MapHeight)
		throw std::ios_base::failure("Map data is corrupted.");
	// Store map in 2d array.
	mapState = std::vector<std::vector<TileType>>(MapHeight, std::vector<TileType>(MapWidth));
	for (int i = 0; i < MapHeight; i++) {
		for (int j = 0; j < MapWidth; j++) {
			const int num = mapData[i * MapWidth + j];
			mapState[i][j] = num ? TILE_FLOOR : TILE_DIRT;
			if (num)
				TileMapGroup->AddNewObject(new Engine::Image("play/floor.png", j * BlockSize, i * BlockSize, BlockSize, BlockSize));
			else
				TileMapGroup->AddNewObject(new Engine::Image("play/dirt.png", j * BlockSize, i * BlockSize, BlockSize, BlockSize));
		}
	}
}
void PlayScene::ReadEnemyWave() {
	std::string filename = std::string("resources/enemy") + std::to_string(MapId) + ".txt";
	// Read enemy file.
	float type, wait, repeat;
	enemyWaveData.clear();
	std::ifstream fin(filename);
	while (fin >> type && fin >> wait && fin >> repeat) {
		for (int i = 0; i < repeat; i++)
			enemyWaveData.emplace_back(type, wait);
	}
	fin.close();
}
void PlayScene::ConstructUI() {
	// Background
	UIGroup->AddNewObject(new Engine::Image("play/sand.png", 1280, 0, 320, 832));
	// Text
	UIGroup->AddNewObject(new Engine::Label(std::string("Stage ") + std::to_string(MapId), "pirulen.ttf", 32, 1294, 0));
	UIGroup->AddNewObject(UIMoney = new Engine::Label(std::string("$") + std::to_string(money), "pirulen.ttf", 24, 1294, 48));
	UIGroup->AddNewObject(UILives = new Engine::Label(std::string("Life ") + std::to_string(lives), "pirulen.ttf", 24, 1294, 88));
	TurretButton* btn;
	// Button 1
	btn = new TurretButton("play/floor.png", "play/dirt.png",
		Engine::Sprite("play/tower-base.png", 1294, 136, 0, 0, 0, 0),
		Engine::Sprite("play/turret-1.png", 1294, 136 - 8, 0, 0, 0, 0)
		, 1294, 136, MachineGunTurret::Price);
	// Reference: Class Member Function Pointer and std::bind.
	btn->SetOnClickCallback(std::bind(&PlayScene::UIBtnClicked, this, 0));
	UIGroup->AddNewControlObject(btn);
	// Button 2
	btn = new TurretButton("play/floor.png", "play/dirt.png",
		Engine::Sprite("play/tower-base.png", 1370, 136, 0, 0, 0, 0),
		Engine::Sprite("play/turret-2.png", 1370, 136 - 8, 0, 0, 0, 0)
		, 1370, 136, LaserTurret::Price);
	btn->SetOnClickCallback(std::bind(&PlayScene::UIBtnClicked, this, 1));
	UIGroup->AddNewControlObject(btn);
	// Button 3
	btn = new TurretButton("play/floor.png", "play/dirt.png",
		Engine::Sprite("play/tower-base.png", 1446, 136, 0, 0, 0, 0),
		Engine::Sprite("play/turret-3.png", 1446, 136, 0, 0, 0, 0)
		, 1446, 136, MissileTurret::Price);
	btn->SetOnClickCallback(std::bind(&PlayScene::UIBtnClicked, this, 2));
	UIGroup->AddNewControlObject(btn);
	// Button 4
	btn = new TurretButton("play/floor.png", "play/dirt.png",
		Engine::Sprite("play/tower-base.png", 1522, 136, 0, 0, 0, 0),
		Engine::Sprite("play/turret-4.png", 1522, 136+8, 0, 0, 0, 0)
		, 1522, 136, MachineGunTurret_model2::Price);
	btn->SetOnClickCallback(std::bind(&PlayScene::UIBtnClicked, this, 3));
	UIGroup->AddNewControlObject(btn);
}

void PlayScene::UIBtnClicked(int id) {
	if (preview)
		UIGroup->RemoveObject(preview->GetObjectIterator());
	if (id == 0 && money >= MachineGunTurret::Price)
		preview = new MachineGunTurret(0, 0);
	else if (id == 1 && money >= LaserTurret::Price)
		preview = new LaserTurret(0, 0);
	else if (id == 2 && money >= MissileTurret::Price)
		preview = new MissileTurret(0, 0);
	else if (id == 3 && money >= MachineGunTurret_model2::Price)
        preview = new MachineGunTurret_model2(0, 0);
	if (!preview)
		return;
	preview->Position = Engine::GameEngine::GetInstance().GetMousePosition();
	preview->Tint = al_map_rgba(255, 255, 255, 200);
	preview->Enabled = false;
	preview->Preview = true;
	UIGroup->AddNewObject(preview);
	OnMouseMove(Engine::GameEngine::GetInstance().GetMousePosition().x, Engine::GameEngine::GetInstance().GetMousePosition().y);
}

bool PlayScene::CheckSpaceValid(int x, int y) {
	if (x < 0 || x >= MapWidth || y < 0 || y >= MapHeight)
		return false;
	auto map00 = mapState[y][x];
	mapState[y][x] = TILE_OCCUPIED;
	std::vector<std::vector<int>> map = CalculateBFSDistance();
	mapState[y][x] = map00;
	if (map[0][0] == -1)
		return false;
	for (auto& it : EnemyGroup->GetObjects()) {
		Engine::Point pnt;
		pnt.x = floor(it->Position.x / BlockSize);
		pnt.y = floor(it->Position.y / BlockSize);
		if (pnt.x < 0 || pnt.x >= MapWidth || pnt.y < 0 || pnt.y >= MapHeight)
			continue;
		if (map[pnt.y][pnt.x] == -1)
			return false;
	}
	// All enemy have path to exit.
	mapState[y][x] = TILE_OCCUPIED;
	mapDistance = map;
	for (auto& it : EnemyGroup->GetObjects())
		dynamic_cast<Enemy*>(it)->UpdatePath(mapDistance);
	return true;
}
std::vector<std::vector<int>> PlayScene::CalculateBFSDistance() {
	// Reverse BFS to find path.
	std::vector<std::vector<int>> map(MapHeight, std::vector<int>(std::vector<int>(MapWidth, -1)));
	std::queue<Engine::Point> que;
	// Push end point.
	// BFS from end point.
	if (mapState[MapHeight - 1][MapWidth - 1] != TILE_DIRT)
		return map;
	que.push(Engine::Point(MapWidth - 1, MapHeight - 1));
	map[MapHeight - 1][MapWidth - 1] = 0;
	while (!que.empty()) {
		Engine::Point p = que.front();
		que.pop();
        /*for (all vertices w adjacent to v)
            if (!visited [w]) {
            q.Push (w);
            visited[w] = true;
            }
        } */
	    if(p.x-1 >= 0 && mapState[p.y][p.x-1] == TILE_DIRT && map[p.y][p.x-1]==-1){
            map[p.y][p.x-1] = map[p.y][p.x]+1;
            que.push(Engine::Point(p.x-1,p.y));
			// std::cout << "map if test2"<<endl;
        }
        if(p.y-1 >= 0 && mapState[p.y-1][p.x] == TILE_DIRT &&map[p.y-1][p.x] == -1){
            map[p.y-1][p.x] = map[p.y][p.x]+1;
            que.push(Engine::Point(p.x,p.y-1));
            // std::cout << "map if test2"<<endl;
        }
        if(p.y+1<MapHeight && mapState[p.y+1][p.x]==TILE_DIRT &&map[p.y+1][p.x]==-1){
            map[p.y+1][p.x] = map[p.y][p.x]+1;
            que.push(Engine::Point(p.x,p.y+1));
            // std::cout << "map if test3"<<endl;
        }
        if(p.x+1<MapWidth && mapState[p.y][p.x+1]==TILE_DIRT &&map[p.y][p.x+1]==-1){
            map[p.y][p.x+1] = map[p.y][p.x]+1;
            que.push(Engine::Point(p.x+1,p.y));
            // std::cout << "map if test4"<<endl;
        }
        else{
            // throw std::logic_error("CalculateBFSDistance is not implemented yet.");
        }
	}
	return map;
}