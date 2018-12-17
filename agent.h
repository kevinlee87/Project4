#pragma once
#include <string>
#include <random>
#include <sstream>
#include <map>
#include <type_traits>
#include <algorithm>
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
	weight_agent(const std::string& args = "") : agent(args) {
		if (meta.find("init") != meta.end()) // pass init=... to initialize the weight
			init_weights(meta["init"]);
		if (meta.find("load") != meta.end()) // pass load=... to load from a specific file
			load_weights(meta["load"]);
	}
	virtual ~weight_agent() {
		if (meta.find("save") != meta.end()) // pass save=... to save to a specific file
			save_weights(meta["save"]);
	}

protected:
	virtual void init_weights(const std::string& info) {
		net.emplace_back(65536); // create an empty weight table with size 65536
		net.emplace_back(65536); // create an empty weight table with size 65536
		// now net.size() == 2; net[0].size() == 65536; net[1].size() == 65536
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
};

/**
 * base agent for agents with a learning rate
 */
class learning_agent : public agent {
public:
	learning_agent(const std::string& args = "") : agent(args), alpha(0.01f) {
		if (meta.find("alpha") != meta.end())
			alpha = float(meta["alpha"]);
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
int pre_slide;
 
class rndenv : public random_agent {
public:
	rndenv(const std::string& args = "") : random_agent("name=random role=environment " + args),
		space({ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 }), popup(0, 9) {}

	virtual action take_action(const board& after) {
		if(pre_slide == -1){
			std::shuffle(space.begin(), space.end(), engine);
			for (int pos : space) {
				if (after(pos) != 0) continue;
				board::cell tile = bag[order[current]];
				current++;
				if(current == 3)	reset();
				return action::place(pos, tile);
			}
		}
		else{
			long long num_element = board_record.size();
			long long MAXSIZE = board_record.capacity();
			if(num_element == MAXSIZE){
				board_record.resize(MAXSIZE + board_size);
			}
			for(int i = 0;i < 16;i++){
				board_record[num_element + i] = after.cell(i); //take elements from board
			}
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
			for (int pos : opposite) { 
				if (after(pos) != 0) continue;
				board::cell tile = bag[order[current]];
				current++;
				if(current == 3)	reset();
				return action::place(pos, tile);
			}
		}
		return action();
	}
	void reset(){
		bag = {1, 2, 3};
		order = {0, 1, 2};
		std::shuffle(order.begin(), order.end(), engine);
		current = 0;
		board_size = 16;
		board_size.resize(board_size);
	}

protected:
	std::array<int, 3> bag;
	std::array<int, 3> order;
	std::array<int, 4> opposite;
	std::array<int, 16> space;
	std::uniform_int_distribution<int> popup;
	int current;
	vector <int> board_record;
	int board_size;
};

/**
 * dummy player
 * select a legal action randomly
 */
 
class player : public weight_agent {
public:
	player(const std::string& args = "") : weight_agent("name=dummy role=player " +args),
		opcode({ 2, 3, 1, 0 }) {}

	virtual action take_action(const board& before) {
		board::max_reward;
		int op_select;
		float value;
		long long key;
		max_reward = -1;
		int 6_tuple[32][6]={{0,1,2,3,4,5}, {3,7,11,15,2,6}, {15,14,13,12,11,10}, {12,8,4,0,13,9}, {3,2,1,0,7,6}, {0,4,8,12,1,5}, {12,13,14,15,8,9}, {15,11,7,3,14,10},
							{4,5,6,7,8,9}, {2,6,10,14,1,5}, {11,10,9,8,7,6}, {13,9,5,1,14,10}, {7,6,5,4,11,10}, {1,5,9,13,2,6}, {8,9,10,11,4,5}, {14,10,6,2,13,9},
							{0,1,2,4,5,6}, {3,7,11,2,6,10}, {15,14,13,11,10,9}, {12,8,4,13,9,5}, {3,2,1,7,6,5}, {0,4,8,1,5,9}, {12,13,14,8,9,10}, {15,11,7,14,10,6},
							{4,5,6,8,9,10}, {2,6,10,1,5,9}, {11,10,9,7,6,5}, {13,9,5,14,10,6}, {7,6,5,11,10,9}, {1,5,9,2,6,10}, {8,9,10,4,5,6}, {14,10,6,13,9,5}}
		for (int op : opcode) {
			board::reward reward = board(before).slide(op);
			if (reward != -1){
				value = 0;
				for(int i = 0;i < 32;i++){
					key = 0;
					for(int j = 0;j < 6;j++){
						key += (long long)1 <<　( before.cell(6_tuple[i][j]) * 4 * (6 - j));
					}
					value += net[i][key];
				}
				reward = reward + value;
				if(max_reward < reward){
					max_reward = reward;
					op_select = op;
				}
			}
		}
		if(max_reward != -1){
			board::reward reward = board(before).slide(op_select);
			return action::slide(op_select);
		}
		return action();
	}
	void reset(){
		pre_slide = -1;
	}

private:
	std::array<int, 4> opcode;
};
