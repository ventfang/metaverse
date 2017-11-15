/**
 * Copyright (c) 2016-2017 mvs developers 
 *
 * This file is part of metaverse-explorer.
 *
 * metaverse-explorer is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Affero General Public License with
 * additional permissions to the one published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version. For more information see LICENSE.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <boost/property_tree/ptree.hpp>      
#include <boost/property_tree/json_parser.hpp>
#include <chrono>
#include <iostream>

#include <metaverse/bitcoin.hpp>
#include <metaverse/client.hpp>
#include <metaverse/explorer/define.hpp>
#include <metaverse/explorer/callback_state.hpp>
#include <metaverse/explorer/display.hpp>
#include <metaverse/explorer/prop_tree.hpp>
#include <metaverse/explorer/dispatch.hpp>
#include <metaverse/explorer/generated.hpp>
#include <metaverse/explorer/extensions/wallet/getnewaddress.hpp>
#include <metaverse/explorer/extensions/command_extension_func.hpp>
#include <metaverse/explorer/extensions/command_assistant.hpp>
#include <metaverse/explorer/extensions/exception.hpp>

namespace libbitcoin {
namespace explorer {
namespace commands {

namespace pt = boost::property_tree;

#define IN_DEVELOPING "this command is in deliberation, or replace it with original command."

/************************ getnewaddress *************************/

///*
console_result getnewaddress::invoke (std::ostream& output,
        std::ostream& cerr, libbitcoin::server::server_node& node)
{
    auto& blockchain = node.chain_impl();
    auto acc = blockchain.is_account_passwd_valid(auth_.name, auth_.auth);
    std::string mnemonic;
    acc->get_mnemonic(auth_.auth, mnemonic);
    if (mnemonic.empty()) { throw mnemonicwords_empty_exception("mnemonic empty"); }
    if (!option_.count) { throw address_amount_exception("invalid address number parameter"); }
    
    const char* cmds[]{"mnemonic-to-seed", "hd-new", "hd-to-ec", "ec-to-public", "ec-to-address"};
    std::stringstream sout("");
    std::istringstream sin(mnemonic);

    auto exec_with = [&](int i){
        sin.str(sout.str());
        sout.str("");
        return dispatch_command(1, cmds + i, sin, sout, sout);
    };

    uint32_t idx = 0;
    pt::ptree aroot;
    pt::ptree addresses;
     
    auto start_timept = std::chrono::high_resolution_clock::now();
    for ( idx = 0; idx < option_.count; idx++ ) {

        auto addr = std::make_shared<bc::chain::account_address>();
        addr->set_name(auth_.name);
        
        sout.str("");
        sin.str(mnemonic);
        if (dispatch_command(1, cmds + 0, sin, sout, sout) != console_result::okay) {
            throw mnemonicwords_to_seed_exception(sout.str());
        }
        relay_exception(sout);
        
        if (exec_with(1) != console_result::okay) {
            throw hd_new_exception(sout.str());
        }
         
        relay_exception(sout);

        auto&& argv_index = std::to_string(acc->get_hd_index());
        const char* hd_private_gen[3] = {"hd-private", "-i", argv_index.c_str()};
        sin.str(sout.str());
        sout.str("");

        if (dispatch_command(3, hd_private_gen, sin, sout, sout) != console_result::okay) {
            throw hd_private_new_exception(sout.str());
        }
         
        relay_exception(sout);

        if (exec_with(2) != console_result::okay) {
            throw hd_to_ec_exception(sout.str());
        }
         
        relay_exception(sout);

        addr->set_prv_key(sout.str(), auth_.auth);
        // not store public key now
        if (exec_with(3) != console_result::okay) {
            throw ec_to_public_exception(sout.str());
        }
         
        relay_exception(sout);

        //addr->set_pub_key(sout.str());

        // testnet
        if (blockchain.chain_settings().use_testnet_rules){
            const char* cmds_tn[]{"ec-to-address", "-v", "127"};
            sin.str(sout.str());
            sout.str("");
            if (dispatch_command(3, cmds_tn, sin, sout, sout) != console_result::okay) {
                throw ec_to_address_exception(sout.str());
            }
             
            relay_exception(sout);

        // mainnet
        } else {
            if (exec_with(4) != console_result::okay) {
                throw ec_to_address_exception(sout.str());
            }
             
            relay_exception(sout);
        }

        addr->set_address(sout.str());
        addr->set_status(1); // 1 -- enable address
        //output<<sout.str();

        acc->increase_hd_index();
        addr->set_hd_index(acc->get_hd_index());
        blockchain.store_account(acc);
        blockchain.store_account_address(addr);
        // write to output json
        pt::ptree address;
        address.put("", sout.str());
        addresses.push_back(std::make_pair("", address));
    }
    auto end_timept = std::chrono::high_resolution_clock::now();
    auto time_elapse = std::chrono::duration_cast<std::chrono::milliseconds>(end_timept - start_timept);
    log::info("CMD") << "time elapse for get " << option_.count << " new addresses is " << time_elapse.count() << " ms";
    
    aroot.add_child("addresses", addresses);
    if(option_.count == 1)
        output<<sout.str();
    else
        pt::write_json(output, aroot);
    
    return console_result::okay;
}
//*/
/*
console_result getnewaddress::invoke(std::ostream& output,
    std::ostream& cerr, libbitcoin::server::server_node& node)
{
    auto& blockchain = node.chain_impl();
    auto acc = blockchain.is_account_passwd_valid(auth_.name, auth_.auth);
    std::string mnemonic;
    acc->get_mnemonic(auth_.auth, mnemonic);
    if (mnemonic.empty()) { throw mnemonicwords_empty_exception("mnemonic empty"); }
    if (!option_.count) { throw address_amount_exception("invalid address number parameter"); }

    std::stringstream sout;
    mnemonic_to_seed cmd_mnemonic_to_seed;
    hd_new cmd_hd_new;
    hd_private cmd_hd_private;
    hd_to_ec cmd_hd_to_ec;
    ec_to_public cmd_ec_to_public;
    ec_to_address cmd_ec_to_address;
    
    uint32_t idx = 0;
    pt::ptree aroot;
    pt::ptree addresses;

    std::vector<std::string> mnemonic_words;
    boost::split(mnemonic_words, mnemonic, boost::is_any_of(" "));

    auto start_timept = std::chrono::high_resolution_clock::now();
    for (idx = 0; idx < option_.count; idx++) {

        auto addr = std::make_shared<bc::chain::account_address>();
        addr->set_name(auth_.name);

        sout.str("");
        cmd_mnemonic_to_seed.set_words_argument(mnemonic_words);
        cmd_mnemonic_to_seed.invoke(sout, sout);

        cmd_hd_new.set_version_option(76066276);
        cmd_hd_new.set_seed_argument(sout.str());
        sout.str("");
        cmd_hd_new.invoke(sout, sout);

        cmd_hd_private.set_hd_private_key_argument(sout.str());
        cmd_hd_private.set_hard_option(false);
        cmd_hd_private.set_index_option(acc->get_hd_index());
        sout.str("");
        cmd_hd_private.invoke(sout, sout);

        cmd_hd_to_ec.set_hd_key_argument(sout.str());
        cmd_hd_to_ec.set_secret_version_option(76066276);
        cmd_hd_to_ec.set_public_version_option(76067358);
        sout.str("");
        cmd_hd_to_ec.invoke(sout, sout);

        addr->set_prv_key(sout.str(), auth_.auth);

        cmd_ec_to_public.set_ec_private_key_argument(sout.str());
        cmd_ec_to_public.set_uncompressed_option(false);
        sout.str("");
        cmd_ec_to_public.invoke(sout, sout);

        cmd_ec_to_address.set_ec_public_key_argument(sout.str());
        // testnet
        if (blockchain.chain_settings().use_testnet_rules) {
            cmd_ec_to_address.set_version_option(127);
        }
        else {
            cmd_ec_to_address.set_version_option(50);
        }
        sout.str("");
        cmd_ec_to_address.invoke(sout, sout);

        addr->set_address(sout.str());
        addr->set_status(1); // 1 -- enable address
                             //output<<sout.str();

        acc->increase_hd_index();
        addr->set_hd_index(acc->get_hd_index());

        blockchain.store_account(acc);
        blockchain.store_account_address(addr);

        // write to output json
        pt::ptree address;
        address.put("", sout.str());
        addresses.push_back(std::make_pair("", address));
    }

    auto end_timept = std::chrono::high_resolution_clock::now();
    auto time_elapse = std::chrono::duration_cast<std::chrono::milliseconds>(end_timept - start_timept);
    log::info("CMD") << "time elapse for get " << option_.count << " new addresses is " << time_elapse.count() << " ms";

    aroot.add_child("addresses", addresses);
    if (option_.count == 1)
        output << sout.str();
    else
        pt::write_json(output, aroot);

    return console_result::okay;
}
*/

} // namespace commands
} // namespace explorer
} // namespace libbitcoin

