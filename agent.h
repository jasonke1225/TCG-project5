/**
 * Framework for NoGo and similar games (C++ 11)
 * agent.h: Define the behavior of variants of the player
 *
 * Author: Theory of Computer Games (TCG 2021)
 *         Computer Games and Intelligence (CGI) Lab, NYCU, Taiwan
 *         https://cgilab.nctu.edu.tw/
 */

#pragma once
#include <string>
#include <random>
#include <sstream>
#include <map>
#include <type_traits>
#include <algorithm>
#include "board.h"
#include "action.h"
#include <fstream>
#include <vector>
#include <memory>

class node {
public:
	node(board state = board(), int value = 0, int nb = 0){
		this->state = state;
		this->value = value;
		this->nb = nb;
	}
	~node(){
		// std::cout<<"bye bye"<<std::endl;
	}

	void addNewChild(int action, std::shared_ptr<node> theChild){
		this->child[action] = theChild;
	}

	void updateValue(int value){
		this->value = this->value + value;
		this->nb++;
	}

	std::shared_ptr<node> findNode(board state){
		for(auto iter = this->child.begin();iter!=this->child.end();iter++){
			if(iter->second->state == state){
				return iter->second;
			}
		}
		return std::make_shared<node>(state,0,1);
	}

public:	
	board state;
	int value;
	int nb;
	std::unordered_map<int ,std::shared_ptr<node>> child;
};

using node_p = std::shared_ptr<node>;

class agent {
public:
	agent(const std::string& args = "") {
		std::stringstream ss("name=unknown role=unknown " + args);
		for (std::string pair; ss >> pair; ) {
			std::string key = pair.substr(0, pair.find('='));
			std::string value = pair.substr(pair.find('=') + 1);
			meta[key] = { value };
			// std::cout<<key<<std::endl;
			// std::cout<<value<<std::endl;
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

/**
 * base agent for agents with randomness
 */
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
 * random player for both side
 * put a legal piece randomly
 */
class player : public random_agent {
public:
	player(const std::string& args = "") : random_agent("name=random role=unknown " + args),
		space(board::size_x * board::size_y), who(board::empty) {
		if (name().find_first_of("[]():; ") != std::string::npos)
			throw std::invalid_argument("invalid name: " + name());
		if (role() == "black") who = board::black;
		if (role() == "white") who = board::white;
		if (who == board::empty)
			throw std::invalid_argument("invalid role: " + role());
		for (size_t i = 0; i < space.size(); i++)
			space[i] = action::place(i, who);
		

		if (meta.find("search") != meta.end()){
			playername = property("search");
			root = std::make_shared<node>(board(),0,1);
		}
		move_space.resize(board::size_x * board::size_y);
		for (size_t i = 0; i < space.size(); i++){
			move_space[i] = i;
		}

	}

	virtual action take_action(const board& state) {
		if (playername == "MCTS"){
			int maxN = 3000;
			board::piece_type who_now = who;
			// root = std::make_shared<node>(state,0,1);
			root = root->findNode(state);

			int empty_place = 0;
			for(int& move_idx : move_space){
				board after = state;
				action::place move = action::place(move_idx, who_now);
				if (move.apply(after) == board::legal){
					empty_place++;
				}
			}

			if(empty_place > 60){
				maxN = 70000;
			}
			else if(empty_place > 30){
				maxN = 100000;
			}
			else if(empty_place > 20){
				maxN = 90000;
			}
			else if(empty_place > 10){
				maxN = 70000;
			}
			else if(empty_place > 0){
				maxN = 50000;
			}

			for(int N=0;N<maxN;N++){
	
				// playOneSequence Function
				std::vector<node_p> trajectory;
				trajectory.push_back(root);
				who_now = who;
				int t_idx = 0;
				float max_v = -1;
				while(trajectory[t_idx]->nb != 0){

					//descendByUCB1(node*)
					node_p nextNode;
					max_v = -INFINITY;
					auto childs = trajectory[t_idx]->child;
					
					std::shuffle(move_space.begin(), move_space.end(), engine);
					for(int& move_idx : move_space){

						if(childs.find(move_idx) != childs.end()){
							float the_v = -childs[move_idx]->value/childs[move_idx]->nb + sqrt(2.0*log((float)trajectory[t_idx]->nb))/childs[move_idx]->nb;
							if (the_v > max_v){
								max_v = the_v;
								nextNode = childs[move_idx];
							}
						}
						else{
							board after = trajectory[t_idx]->state;
							action::place move = action::place(move_idx, who_now);
							if (move.apply(after) == board::legal){
								nextNode = std::make_shared<node>(after,0,0);
								trajectory[t_idx]->addNewChild(move_idx, nextNode);

								//simulation
								bool loose = false;
								auto sim_who = who_change(who_now);
								auto sim_space = move_space;
								auto sim_state = after;
								while( !loose ){
									loose = true;
									std::shuffle(sim_space.begin(), sim_space.end(), engine);
									for(int& sim_idx : sim_space){
										action::place sim_move = action::place(sim_idx, sim_who);
										if(sim_move.apply(sim_state) == board::legal){
											sim_who = who_change(sim_who);
											loose = false;
											break;
										}
									}
								}
								if(sim_who != who){
									max_v = 1;
								}
								else{
									max_v = 0;
								}
								/*********************/
								break;
							}
						}
					}
					
					if(max_v==-INFINITY){
						if(who_now != who)
							max_v = 1;
						else
							max_v = 0;
						break;
					}
					trajectory.push_back(nextNode);
					/***********************/
					t_idx++;
					who_now = who_change(who_now);
				}
				// std::cout<<"the win"<<" : "<<max_v<<std::endl;
				//updateValue(node*, -node[i]->value)
				if( t_idx % 2 == 1){
					// record not who's turn
					max_v *= -1;
				}
				for(t_idx = t_idx; t_idx>=0; t_idx--){
					trajectory[t_idx]->updateValue(max_v);
					// std::cout<<t_idx<<" : "<<trajectory[t_idx]->value<<std::endl;
					max_v = -max_v;
				}
			}

			//	Get action
			float max_v = -INFINITY;
			int max_idx = 0;
			auto childs = root->child;
			for(auto iter = childs.begin(); iter!=childs.end();iter++){
				auto move_idx = iter->first;
				float the_v = -(float)childs[move_idx]->value/childs[move_idx]->nb;// + sqrt(2*log10(root->nb)/childs[move_idx]->nb);
				if (the_v > max_v){
					max_v = the_v;
					max_idx = move_idx;
				}
			}
			if(root->child.find(max_idx)!=root->child.end())
				root = root->child[max_idx];

			// std::cout<<action::place(max_idx, who).position()<<std::endl;

			return action::place(max_idx, who);
		}
		else{
			std::shuffle(space.begin(), space.end(), engine);
			for (const action::place& move : space) {
				board after = state;
				// std::cout<<after<<std::endl;
				// int a = 0;
				if (move.apply(after) == board::legal)
					// std::cout<<a++<<" : "<<after<<std::endl;
					return move;
			}
			return action();
		}
	}

	virtual board::piece_type who_change(board::piece_type who_now){
		if(who_now == board::black)
			who_now = board::white;
		else if(who_now == board::white)
			who_now = board::black;

		return who_now;
	}

private:
	std::vector<action::place> space;
	std::vector<int> move_space;
	board::piece_type who;
	std::string playername;
	node_p root;
};

