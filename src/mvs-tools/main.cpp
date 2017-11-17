/**
 * Copyright (c) 2011-2015 libbitcoin developers (see AUTHORS)
 * Copyright (c) 2016-2017 metaverse core developers (see MVS-AUTHORS)
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
#include <iostream>

#include <boost/filesystem.hpp>

#include <metaverse/bitcoin/utility/path.hpp>
#include <metaverse/bitcoin/wallet/settings.hpp>
#include <metaverse/database/databases/account_database.hpp>

using namespace libbitcoin;

inline hash_digest get_hash(const std::string str)
{
    data_chunk data(str.begin(), str.end());
    return sha256_hash(data);
}

int main(int argc, char* argv[])
{
    auto home_path = default_data_path();
    auto data_path = home_path / "mainnet/account_table";
    auto module = boost::filesystem::path(argv[0]).filename().string();
    if (argc < 3) {
        std::cout << "Usage: " << module << " username password [\"path_to_account_table\"]" << std::endl;
        std::cout << "        default file is: \"" << data_path.generic_string() << "\"" << std::endl;
        std::cout << "        eg. " << module << " test 123456" << std::endl;
        return -1;
    }
    hash_digest name_hash = get_hash(argv[1]);
    hash_digest passwd_hash = get_hash(argv[2]);
    if (argc > 3)
        data_path = argv[3];

    if (!boost::filesystem::exists(data_path)) {
        std::cout << "database not found at " << data_path.string() << std::endl;
        return -1;
    }
    std::cout << "load accounts from " << data_path.generic_string() << std::endl;
    std::shared_ptr<shared_mutex> mutex = std::make_shared<shared_mutex>();
    database::account_database accounts(data_path, mutex);
    accounts.start();
    auto vec_accs = accounts.get_accounts();
    //std::cout << "loaded " << vec_accs->size() << " accounts." << std::endl;
    for (auto& vec : *vec_accs) {
        if (vec.get_name() != argv[1]) {
            continue;
        }
        if (vec.get_passwd() != passwd_hash) {
            return 0;
        }
        std::cout << "Account: " << vec.get_name() << std::endl;
        std::string mnemonic;
        std::string passwd = std::string(argv[2]);
        std::cout << "Mnemonic: " << vec.get_mnemonic(passwd, mnemonic) << std::endl << std::endl;
        return 0;
    }
    std::cout << "Account" << argv[1] << " not found !!!" << std::endl;
    return 0;
}

