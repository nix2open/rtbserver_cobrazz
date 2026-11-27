add_executable(BidPriceTest
  BidPriceTest.cpp
  ${CMAKE_SOURCE_DIR}/Frontends/Modules/BiddingFrontend/BidPrice.cpp)

target_link_libraries(BidPriceTest PRIVATE Generics String)
target_include_directories(BidPriceTest PRIVATE ${CMAKE_SOURCE_DIR})
