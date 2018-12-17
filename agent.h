#pragma once
#include <string>
#include <random>
#include <sstream>
#include <map>
#include <type_traits>
#include <algorithm>
#include <vector>
#include "board.h"
#include "action.h"
#include "weight.h"
#include <fstream>

class agent {
public:
	agent(const std::string& args = "") {
		std::stringstream ss("name=unknown role=unknown " + args);
		for (std::string pair; ss >> pair; ) {
			std::string key = pair.substr(0, pair.find('='));
			std::string value = pair.substr(pair.find('=') + 1);
			meta[key] = { value };
		}
	}
	virtual ~agent() {}
	virtual void open_episode(const std::string& flag = "") {}
	virtual void close_episode(const std::string& flag = "") {}
	virtual action take_action(const board& b) { return action(); }
	virtual bool check_for_win(const board& b) { return false; }

public:
	virtual std::string property(const std::string& key) const { return meta.at(key); }
	virtual void notify(const std::string& msg) { meta[msg.substr(0, msg.find('='))] = { msg.substr(msg.find('=') + 1) }; }
	virtual std::string name() const { return property("name"); }
	virtual std::string role() const { return property("role"); }

protected:
	typedef std::string key;
	struct value {
		std::string value;
		operator std::string() const { return value; }
		template<typename numeric, typename = typename std::enable_if<std::is_arithmetic<numeric>::value, numeric>::type>
		operator numeric() const { return numeric(std::stod(value)); }
	};
	std::map<key, value> meta;
};

class random_agent : public agent {
public:
	random_agent(const std::string& args = "") : agent(args) {
		if (meta.find("seed") != meta.end())
			engine.seed(int(meta["seed"]));
	}
	virtual ~random_agent() {}

protected:
	std::default_random_engine engine;
};

/**
 * base agent for agents with weight tables
 */
class weight_agent : public agent {
public:
	weight_agent(const std::string& args = "") : agent(args), alpha(0.005f){
		if (meta.find("init") != meta.end()) // pass init=... to initialize the weight
			init_weights(meta["init"]);
		if (meta.find("load") != meta.end()) // pass load=... to load from a specific file
			load_weights(meta["load"]);
		if (meta.find("alpha") != meta.end()){
			alpha = float(meta["alpha"]);
		}
	}
	virtual ~weight_agent() {
		if (meta.find("save") != meta.end()) // pass save=... to save to a specific file
			save_weights(meta["save"]);
	}

protected:
	virtual void init_weights(const std::string& info) {
		net.emplace_back(0xFFFFFFF * 2); // create an empty weight table with size 1342177280
		net.emplace_back(0xFFFFFFF * 2); // create an empty weight table with size 1342177280
		net.emplace_back(0xFFFFFFF * 2); // create an empty weight table with size 1342177280
		net.emplace_back(0xFFFFFFF * 2); // create an empty weight table with size 1342177280
	}
	virtual void load_weights(const std::string& path) {
		std::ifstream in(path, std::ios::in | std::ios::binary);
		if (!in.is_open()) std::exit(-1);
		uint32_t size;
		in.read(reinterpret_cast<char*>(&size), sizeof(size));
		net.resize(size);
		for (weight& w : net) in >> w;
		in.close();
	}
	virtual void save_weights(const std::string& path) {
		std::ofstream out(path, std::ios::out | std::ios::binary | std::ios::trunc);
		if (!out.is_open()) std::exit(-1);
		uint32_t size = net.size();
		out.write(reinterpret_cast<char*>(&size), sizeof(size));
		for (weight& w : net) out << w;
		out.close();
	}

protected:
	std::vector<weight> net;
	float alpha;
};

/**
 * base agent for agents with a learning rate
 */
class learning_agent : public agent {
public:
	learning_agent(const std::string& args = "") : agent(args), alpha(0.1f){
		if (meta.find("alpha") != meta.end()){
			alpha = float(meta["alpha"]);
		}
	}
	virtual ~learning_agent() {}

protected:
	float alpha;
};

/**
 * random environment
 * add a new random tile to an empty cell
 * 2-tile: 90%
 * 4-tile: 10%
 */
int pre_slide, hint_tile;
 
class rndenv : public random_agent {
public:
	rndenv(const std::string& args = "") : random_agent("name=random role=environment " + args),
		space({ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 }), popup(0, 20) {}

	virtual action take_action(const board& after) {
		int temp, max;
		board::cell tile = 0;
		if(pre_slide == -1){
			count = 0;
			std::shuffle(space.begin(), space.end(), engine);
			for (int pos : space) {
				if (after(pos) != 0) continue;
				board::cell tile = bag[order[current]];
				current++;
				if(current == 12)	reset();
				hint_tile = bag[order[current]];
				return action::place(pos, tile);
			}
		}
		else{
			switch(pre_slide){
				case 0:	//up
					opposite = {12, 13, 14, 15};
					break;
				case 1:	//right
					opposite = {0, 4, 8, 12};
					break;
				case 2:	//down
					opposite = {0, 1, 2, 3};
					break;
				case 3:	//left
					opposite = {3, 7, 11, 15};
					break;
			}
			std::shuffle(opposite.begin(), opposite.end(), engine);
			max = 0;
			for(int i = 0;i < 16;i++){
				temp = after(i);
				if(max < temp){
					max = temp;
				}
			}
			if(bag[order[current]] == hint_tile){
				tile = bag[order[current]];
				current++;
			}
			else	tile = hint_tile;
			if(current == 12)	reset();
			if(max > 6){
				count++;
				if(count == 21){
					std::vector<int> bonus_tile;
					int end = max - 6;
					for(int i = 0;i < end;i++){
						temp = 4 + i;
						bonus_tile.push_back(temp);
					}
					std::shuffle(bonus_tile.begin(), bonus_tile.end(), engine);
					hint_tile = bonus_tile[0];
					count = 0;
				}
				else	hint_tile = bag[order[current]];
			}
			else	hint_tile = bag[order[current]];
			for (int pos : opposite) { 
				if (after(pos) != 0) continue;
				return action::place(pos, tile);
			}
		}
		return action();
	}
	void reset(){
		for(int i = 0;i < 12;i++){
			bag[i] = (i / 4) + 1;
			order[i] = i;
		}
		std::shuffle(order.begin(), order.end(), engine);
		current = 0;
	}

protected:
	std::array<int, 12> bag;
	std::array<int, 12> order;
	std::array<int, 4> opposite;
	std::array<int, 16> space;
	std::uniform_int_distribution<int> popup;
	int current, count;
};

/**
 * dummy player
 * select a legal action randomly
 */
 
class player : public weight_agent {
public:
	player(const std::string& args = "") : weight_agent("name=dummy role=player " +args){}

	virtual action take_action(const board& before) {
		board::reward reward, reward_select;
		board test, after;
		int op_select, key;
		float value, max_value;
		int tuple_6[4][8][6]={{{0,1,2,3,4,5}, {3,7,11,15,2,6}, {15,14,13,12,11,10}, {12,8,4,0,13,9}, {3,2,1,0,7,6}, {0,4,8,12,1,5}, {12,13,14,15,8,9}, {15,11,7,3,14,10}},
							{{4,5,6,7,8,9}, {2,6,10,14,1,5}, {11,10,9,8,7,6}, {13,9,5,1,14,10}, {7,6,5,4,11,10}, {1,5,9,13,2,6}, {8,9,10,11,4,5}, {14,10,6,2,13,9}},
							{{0,1,2,4,5,6}, {3,7,11,2,6,10}, {15,14,13,11,10,9}, {12,8,4,13,9,5}, {3,2,1,7,6,5}, {0,4,8,1,5,9}, {12,13,14,8,9,10}, {15,11,7,14,10,6}},
							{{4,5,6,8,9,10}, {2,6,10,1,5,9}, {11,10,9,7,6,5}, {13,9,5,14,10,6}, {7,6,5,11,10,9}, {1,5,9,2,6,10}, {8,9,10,4,5,6}, {14,10,6,13,9,5}}};
		
		max_value = -2147483648;
		op_select = -1;
		reward_select = -1;
		for (int op : opcode) {
			test = before;
			reward = test.slide(op);
			if (reward != -1){
				value = 0;
				for(int i = 0;i < 4;i++){
					for(int k = 0;k < 8;k++){
						if(pre_slide != -1){
							key = 0;
							key = pre_slide;
							key = key * 12 + hint_tile;
							for(int j = 0;j < 6;j++){
								key = key * 15 + test(tuple_6[i][k][j]);
							}
							value += net[i][key];
						}
						else{
							value = 0;
						}
					}
				}
				value = reward + value;
				if(max_value < value){
					after = test;
					max_value = value;
					op_select = op;
					reward_select = reward;
				}
			}
		}
		if(max_value > -2147483648){
			board_record.push_back(after);
			reward_record.push_back(reward_select);
			hint_tile_record.push_back(hint_tile);
			pre_slide = op_select;
			pre_slide_record.push_back(pre_slide);
			return action::slide(op_select);
		}
		return action();
	}
	void train(){
		if(alpha == 0)	return;
		board::reward reward;
		board cur, pre;
		int pre_hint, cur_hint, pre_pre_slide, cur_pre_slide;
		unsigned long long key;
		float value_cur, value_pre, fix;
		int tuple_6[4][8][6]={{{0,1,2,3,4,5}, {3,7,11,15,2,6}, {15,14,13,12,11,10}, {12,8,4,0,13,9}, {3,2,1,0,7,6}, {0,4,8,12,1,5}, {12,13,14,15,8,9}, {15,11,7,3,14,10}},
							{{4,5,6,7,8,9}, {2,6,10,14,1,5}, {11,10,9,8,7,6}, {13,9,5,1,14,10}, {7,6,5,4,11,10}, {1,5,9,13,2,6}, {8,9,10,11,4,5}, {14,10,6,2,13,9}},
							{{0,1,2,4,5,6}, {3,7,11,2,6,10}, {15,14,13,11,10,9}, {12,8,4,13,9,5}, {3,2,1,7,6,5}, {0,4,8,1,5,9}, {12,13,14,8,9,10}, {15,11,7,14,10,6}},
							{{4,5,6,8,9,10}, {2,6,10,1,5,9}, {11,10,9,7,6,5}, {13,9,5,14,10,6}, {7,6,5,11,10,9}, {1,5,9,2,6,10}, {8,9,10,4,5,6}, {14,10,6,13,9,5}}};

		pre = board_record.back();
		pre_hint = hint_tile_record.back();
		pre_pre_slide = pre_slide_record.back();
		value_pre = 0;
		for(int i = 0;i < 4;i++){
			for(int k = 0;k < 8;k++){
				key = 0;
				key = pre_pre_slide;
				key = key * 12 + pre_hint;
				for(int j = 0;j < 6;j++){
					key = key * 15 + pre(tuple_6[i][k][j]);
				}
				value_pre += net[i][key];
			}
		}
		fix = (0 - value_pre) * alpha / 32;
		for(int i = 0;i < 4;i++){
			for(int k = 0;k < 8;k++){
				key = 0;
				key = pre_pre_slide;
				key = key * 12 + pre_hint;
				for(int j = 0;j < 6;j++){
					key = key * 15 + pre(tuple_6[i][k][j]);
				}
				net[i][key] += fix;
			}
		}
		while(board_record.size() > 1){
			cur = board_record.back();
			board_record.pop_back();
			pre = board_record.back();
			reward = reward_record.back();
			reward_record.pop_back();
			cur_hint = hint_tile_record.back();
			hint_tile_record.pop_back();
			pre_hint = hint_tile_record.back();
			cur_pre_slide = pre_slide_record.back();
			pre_slide_record.pop_back();
			pre_pre_slide = pre_slide_record.back();
			value_cur = 0;
			value_pre = 0;
			for(int i = 0;i < 4;i++){
				for(int k = 0;k < 8;k++){
					key = 0;
					key = cur_pre_slide;
					key = key * 12 + cur_hint;
					for(int j = 0;j < 6;j++){
						key = key * 15 + cur(tuple_6[i][k][j]);
					}
					value_cur += net[i][key];
				}
				for(int k = 0;k < 8;k++){
					key = 0;
					key = pre_pre_slide;
					key = key * 12 + pre_hint;
					for(int j = 0;j < 6;j++){
						key = key * 15 + pre(tuple_6[i][k][j]);
					}
					value_pre += net[i][key];
				}
			}
			fix = (value_cur - value_pre + reward) * alpha / 32;
			for(int i = 0;i < 4;i++){
				for(int k = 0;k < 8;k++){
					key = 0;
					key = pre_pre_slide;
					key = key * 12 + pre_hint;
					for(int j = 0;j < 6;j++){
						key = key * 15 + pre(tuple_6[i][k][j]);
					}
					net[i][key] += fix;
				}
			}
		}
	}
	void reset(){
		pre_slide = -1;
		for(int i = 0;i < 4;i++)	opcode[i] = i;
	}
private:
	std::vector <board> board_record;
	std::vector <float> reward_record;
	std::vector <int> pre_slide_record;
	std::vector <int> hint_tile_record;
	int opcode[4];
};
