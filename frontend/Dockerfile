FROM node:20 AS builder

WORKDIR /build

COPY package.json ./

ENV YARN_HTTP_TIMEOUT=60000
RUN yarn install

COPY . .

RUN yarn build

FROM node:20-alpine

WORKDIR /app

RUN npm install -g serve

COPY --from=builder /build/dist ./dist

EXPOSE 3000

ENV HOST=0.0.0.0
ENV PORT=3000

CMD ["serve", "-s", "dist", "-l", "3000", "--single"]
